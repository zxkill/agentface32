#include "hardware_profile.h"
#if defined(DISPLAY_BACKEND_TFT_ESPI)

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>
#include <pgmspace.h>
#include "face.h"
#include "agent.h"
#include "config.h"
#include "locale.h"
#include "ui_font_data.h"

#ifndef TFT_ROTATION
#define TFT_ROTATION 1
#endif
#ifndef TFT_BL
#define TFT_BL -1
#endif
#ifndef TFT_BACKLIGHT_ON
#define TFT_BACKLIGHT_ON HIGH
#endif

static TFT_eSPI g_tft;
static TFT_eSprite g_sprite(&g_tft);
static bool g_spriteReady = false;
static FaceViewMode g_viewMode = FaceViewMode::Claude;
static FaceViewMode g_lastManualMode = FaceViewMode::Claude;
static String g_ip;
static bool g_voice = true;
static uint32_t g_lastFrame = 0;

static constexpr uint16_t C_BG = TFT_BLACK;
static constexpr uint16_t C_PANEL = 0x1082;
static constexpr uint16_t C_PANEL2 = 0x2945;
static constexpr uint16_t C_TEXT = TFT_WHITE;
static constexpr uint16_t C_MUTED = 0x8410;
static constexpr uint16_t C_CYAN = 0x05FF;
static constexpr uint16_t C_GREEN = 0x07E0;
static constexpr uint16_t C_YELLOW = 0xFFE0;
static constexpr uint16_t C_RED = 0xF800;
static constexpr uint16_t C_PURPLE = 0xA81F;

static uint32_t nextCodepoint(const String& s, size_t& i) {
  if (i >= s.length()) return 0;
  const uint8_t c = (uint8_t)s[i++];
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0 && i < s.length()) {
    uint32_t cp = (c & 0x1F) << 6; cp |= ((uint8_t)s[i++] & 0x3F); return cp;
  }
  if ((c & 0xF0) == 0xE0 && i + 1 < s.length()) {
    uint32_t cp = (c & 0x0F) << 12;
    cp |= (((uint8_t)s[i++] & 0x3F) << 6); cp |= ((uint8_t)s[i++] & 0x3F); return cp;
  }
  while (i < s.length() && (((uint8_t)s[i] & 0xC0) == 0x80)) ++i;
  return '?';
}

static bool findGlyph(const UiFont& font, uint32_t cp, UiGlyph& out) {
  int lo = 0, hi = (int)font.glyphCount - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2; UiGlyph g; memcpy_P(&g, font.glyphs + mid, sizeof(g));
    if (g.codepoint == cp) { out = g; return true; }
    if (g.codepoint < cp) lo = mid + 1; else hi = mid - 1;
  }
  if (cp != '?') return findGlyph(font, '?', out);
  return false;
}

static uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha) {
  if (!alpha) return bg; if (alpha >= 15) return fg;
  int fr=(fg>>11)&31, fg6=(fg>>5)&63, fb=fg&31;
  int br=(bg>>11)&31, bg6=(bg>>5)&63, bb=bg&31, ia=15-alpha;
  return (uint16_t)((((fr*alpha+br*ia+7)/15)<<11) | (((fg6*alpha+bg6*ia+7)/15)<<5) | ((fb*alpha+bb*ia+7)/15));
}

static int textWidth(const UiFont& font, const String& text) {
  int width=0; size_t i=0; while(i<text.length()){UiGlyph g;if(findGlyph(font,nextCodepoint(text,i),g))width+=g.advance;} return width;
}

static void drawText(const UiFont& font, const String& text, int x, int y, uint16_t color, uint16_t bg, int maxWidth=-1) {
  int cursor=x; size_t i=0;
  while(i<text.length()) {
    UiGlyph g; if(!findGlyph(font,nextCodepoint(text,i),g)) continue;
    if(maxWidth>=0 && cursor+g.advance>x+maxWidth) break;
    uint32_t pix=0;
    for(uint8_t row=0;row<g.height;++row){for(uint8_t col=0;col<g.width;++col,++pix){
      uint8_t packed=pgm_read_byte(font.bitmap+g.offset+(pix>>1)); uint8_t a=(pix&1)?(packed&0x0F):(packed>>4);
      if(a) g_sprite.drawPixel(cursor+col,y+row,blend565(color,bg,a));
    }}
    cursor+=g.advance;
  }
}

static void centered(const UiFont& font, const String& text, int cx, int y, uint16_t color, uint16_t bg, int maxWidth=-1) {
  int w=textWidth(font,text); if(maxWidth>=0 && w>maxWidth) w=maxWidth;
  drawText(font,text,cx-w/2,y,color,bg,maxWidth);
}

static String taskClock(uint32_t ms) {
  uint32_t t=ms/1000; char b[14];
  if(t>=3600) snprintf(b,sizeof(b),"%lu:%02lu:%02lu",(unsigned long)(t/3600),(unsigned long)((t/60)%60),(unsigned long)(t%60));
  else snprintf(b,sizeof(b),"%lu:%02lu",(unsigned long)(t/60),(unsigned long)(t%60));
  return String(b);
}

static uint16_t accent(AgentState s) {
  switch(s){case AgentState::Attention:return C_YELLOW;case AgentState::Done:return C_GREEN;case AgentState::Error:case AgentState::Abort:return C_RED;case AgentState::Reading:return C_CYAN;case AgentState::Running:return C_PURPLE;default:return C_TEXT;}
}

static AgentId displayedAgent(){if(g_viewMode==FaceViewMode::Codex)return AgentId::Codex;if(g_viewMode==FaceViewMode::Auto)return agentMostRecent();return AgentId::Claude;}
AgentId faceFocusedAgent(){return g_viewMode==FaceViewMode::Both?agentMostRecent():displayedAgent();}

static void drawFace(AgentState s, uint32_t now, int top, int bottom) {
  int w=g_sprite.width(); int h=bottom-top; int cy=top+h/2-5;
  int ex1=w*30/100, ex2=w*70/100, ew=max(20,w/7), eh=max(13,h/5);
  int gaze=0, gy=0; bool closed=s==AgentState::Sleep;
  if(s==AgentState::Reading) gaze=(int)(ew*0.18f*sinf(now/380.0f));
  if(s==AgentState::Thinking){gaze=ew/6;gy=-eh/5;}
  if(s==AgentState::Typing||s==AgentState::Running)gy=eh/5;
  bool blink=(now%4700)>4580 && s!=AgentState::Attention && s!=AgentState::Done;
  uint16_t a=accent(s);
  if(closed||blink){g_sprite.drawLine(ex1-ew/2,cy,ex1+ew/2,cy,C_MUTED);g_sprite.drawLine(ex2-ew/2,cy,ex2+ew/2,cy,C_MUTED);}
  else if(s==AgentState::Done){
    // Happy closed eyes. Use simple line segments instead of drawArc so the
    // renderer behaves consistently across TFT_eSPI versions.
    g_sprite.drawLine(ex1-ew/2,cy,ex1,cy+4,a); g_sprite.drawLine(ex1,cy+4,ex1+ew/2,cy,a);
    g_sprite.drawLine(ex2-ew/2,cy,ex2,cy+4,a); g_sprite.drawLine(ex2,cy+4,ex2+ew/2,cy,a);
  }
  else {
    g_sprite.drawRoundRect(ex1-ew/2,cy-eh/2,ew,eh,eh/3,C_TEXT);g_sprite.drawRoundRect(ex2-ew/2,cy-eh/2,ew,eh,eh/3,C_TEXT);
    int pr=max(2,eh/7); if(s==AgentState::Attention)pr+=1;
    g_sprite.fillCircle(ex1+gaze,cy+gy,pr,a);g_sprite.fillCircle(ex2+gaze,cy+gy,pr,a);
    int by=cy-eh/2-5;
    if(s==AgentState::Error||s==AgentState::Abort){g_sprite.drawLine(ex1-ew/2,by,ex1+ew/2,by+4,C_RED);g_sprite.drawLine(ex2-ew/2,by+4,ex2+ew/2,by,C_RED);}
    else {g_sprite.drawLine(ex1-ew/2,by,ex1+ew/2,by,C_MUTED);g_sprite.drawLine(ex2-ew/2,by,ex2+ew/2,by,C_MUTED);}
  }
  int my=cy+eh/2+10;
  if(s==AgentState::Done||s==AgentState::Welcome){
    g_sprite.drawLine(w/2-16,my-4,w/2-7,my+2,a);
    g_sprite.drawLine(w/2-7,my+2,w/2+7,my+2,a);
    g_sprite.drawLine(w/2+7,my+2,w/2+16,my-4,a);
  } else if(s==AgentState::Error){
    g_sprite.drawLine(w/2-16,my+5,w/2-7,my-1,C_RED);
    g_sprite.drawLine(w/2-7,my-1,w/2+7,my-1,C_RED);
    g_sprite.drawLine(w/2+7,my-1,w/2+16,my+5,C_RED);
  }
  else if(s==AgentState::Thinking){for(int i=0;i<3;++i)g_sprite.fillCircle(w/2-10+i*10,my,((now/300)%3)==i?2:1,C_TEXT);}
  else g_sprite.drawLine(w/2-12,my,w/2+12,my,C_TEXT);
}

static bool important(AgentState s){return s==AgentState::Attention||s==AgentState::Error||s==AgentState::Abort||s==AgentState::Done;}

static void renderAgent(AgentId agent,uint32_t now){
  int w=g_sprite.width(),h=g_sprite.height(); AgentState s=agentGetState(agent); uint16_t a=accent(s);
  g_sprite.fillSprite(C_BG);g_sprite.fillRoundRect(4,4,w-8,h-8,10,C_PANEL);g_sprite.drawRoundRect(4,4,w-8,h-8,10,C_PANEL2);
  drawText(UI_FONT_HEADER,agentName(agent),12,9,agent==AgentId::Claude?C_CYAN:C_GREEN,C_PANEL,w/2);
  String rt; if(g_viewMode==FaceViewMode::Auto)rt="AUTO"; if(agentTaskActive(agent)){if(rt.length())rt+=" ";rt+=taskClock(agentTaskElapsedMs(agent));}
  if(rt.length())drawText(UI_FONT_SMALL,rt,w-12-textWidth(UI_FONT_SMALL,rt),12,g_viewMode==FaceViewMode::Auto?C_YELLOW:C_CYAN,C_PANEL,w/2);
  int faceTop=30,faceBottom=max(75,h-54);drawFace(s,now,faceTop,faceBottom);
  centered(UI_FONT_BIG,stateTitle(s),w/2,h-47,a,C_BG,w-14);
  String d=agentGetDetail(agent);centered(UI_FONT_SMALL,d,w/2,h-24,C_TEXT,C_BG,w-14);
  AgentId other=agent==AgentId::Claude?AgentId::Codex:AgentId::Claude;
  if(g_viewMode!=FaceViewMode::Auto&&agentHasSeen(other)&&important(agentGetState(other))&&now-agentStateSince(other)<4500){
    g_sprite.fillCircle(w-11,h-11,7,accent(agentGetState(other)));
    drawText(UI_FONT_SMALL,"!",w-14,h-17,C_BG,accent(agentGetState(other)),10);
  }
}

static void renderCard(AgentId a,int y,int height,uint32_t now){
  int w=g_sprite.width(); AgentState s=agentGetState(a);g_sprite.fillRoundRect(7,y,w-14,height-3,8,C_PANEL);g_sprite.drawRoundRect(7,y,w-14,height-3,8,accent(s));
  drawText(UI_FONT_HEADER,agentName(a),14,y+7,a==AgentId::Claude?C_CYAN:C_GREEN,C_PANEL,w/2);
  String st=agentHasSeen(a)?String(stateTitle(s)):String("-");drawText(UI_FONT_SMALL,st,w-14-textWidth(UI_FONT_SMALL,st),y+10,accent(s),C_PANEL,w/2);
  String info;if(agentTaskActive(a))info=taskClock(agentTaskElapsedMs(a))+" / "+String(agentToolCount(a));else if(agentLastTaskDurationMs(a))info=taskClock(agentLastTaskDurationMs(a))+" / "+String(agentLastToolCount(a));else info=agentHasSeen(a)?tr("ready","готов"):tr("no data","нет данных");
  drawText(UI_FONT_SMALL,info,14,y+height-18,C_MUTED,C_PANEL,w-28);
  (void)now;
}

static void renderBoth(uint32_t now){
  int w=g_sprite.width(),h=g_sprite.height();g_sprite.fillSprite(C_BG);centered(UI_FONT_HEADER,tr("AGENTS","АГЕНТЫ"),w/2,7,C_TEXT,C_BG,w-12);
  int top=28;int ch=(h-top-8)/2;renderCard(AgentId::Claude,top,ch,now);renderCard(AgentId::Codex,top+ch,ch,now);
}

void faceBegin(){
#if TFT_BL >= 0
  pinMode(TFT_BL,OUTPUT); digitalWrite(TFT_BL,TFT_BACKLIGHT_ON);
#endif
  g_tft.init();g_tft.setRotation(TFT_ROTATION);g_tft.fillScreen(C_BG);
  g_sprite.setColorDepth(8);g_spriteReady=g_sprite.createSprite(g_tft.width(),g_tft.height())!=nullptr;
  if(g_spriteReady){g_sprite.fillSprite(C_BG);centered(UI_FONT_HEADER,"AgentFace32",g_tft.width()/2,g_tft.height()/2-15,C_CYAN,C_BG,g_tft.width()-12);centered(UI_FONT_SMALL,HARDWARE_PROFILE_NAME,g_tft.width()/2,g_tft.height()/2+10,C_MUTED,C_BG,g_tft.width()-12);g_sprite.pushSprite(0,0);}
}
void faceSetNetwork(const String& ip){g_ip=ip;(void)g_ip;g_lastFrame=0;}
void faceSetVoiceEnabled(bool e){g_voice=e;(void)g_voice;g_lastFrame=0;}
void faceCycleViewMode(){if(g_viewMode==FaceViewMode::Auto)g_viewMode=g_lastManualMode;else if(g_viewMode==FaceViewMode::Claude)g_viewMode=g_lastManualMode=FaceViewMode::Codex;else if(g_viewMode==FaceViewMode::Codex)g_viewMode=g_lastManualMode=FaceViewMode::Both;else g_viewMode=g_lastManualMode=FaceViewMode::Claude;g_lastFrame=0;}
void faceToggleAutoMode(){if(g_viewMode==FaceViewMode::Auto)g_viewMode=g_lastManualMode;else{g_lastManualMode=g_viewMode;g_viewMode=FaceViewMode::Auto;}g_lastFrame=0;}
FaceViewMode faceGetViewMode(){return g_viewMode;}
const char* faceViewModeName(){switch(g_viewMode){case FaceViewMode::Claude:return"claude";case FaceViewMode::Codex:return"codex";case FaceViewMode::Both:return"both";case FaceViewMode::Auto:return"auto";}return"claude";}
bool faceSetViewModeByName(const String& raw){String m(raw);m.toLowerCase();if(m=="claude")g_viewMode=FaceViewMode::Claude;else if(m=="codex")g_viewMode=FaceViewMode::Codex;else if(m=="both"||m=="all")g_viewMode=FaceViewMode::Both;else if(m=="auto")g_viewMode=FaceViewMode::Auto;else return false;if(g_viewMode!=FaceViewMode::Auto)g_lastManualMode=g_viewMode;g_lastFrame=0;return true;}
void faceLoop(){if(!g_spriteReady)return;uint32_t now=millis();if(g_lastFrame&&now-g_lastFrame<66)return;g_lastFrame=now;if(g_viewMode==FaceViewMode::Both)renderBoth(now);else renderAgent(displayedAgent(),now);g_sprite.pushSprite(0,0);}

#endif // DISPLAY_BACKEND_TFT_ESPI
