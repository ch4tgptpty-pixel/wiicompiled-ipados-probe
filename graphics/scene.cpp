#include "scene.h"
#include <dolphin/gx.h>
#include <cmath>

void DrawProbeScene(uint32_t frameIndex, uint32_t width, uint32_t height) {
  // Initialize real GX FIFO state once; the app owns this storage until shutdown.
  alignas(32) static unsigned char fifo[32768];
  static bool initialized=false;
  if(!initialized) { GXInit(fifo,sizeof(fifo)); initialized=true; }
  if(width==0 || height==0) return;
  AuroraSetViewportPolicy(AURORA_VIEWPORT_NATIVE);
  GXSetViewport(0,0,float(width),float(height),0,1);
  // Physical Retina extents exceed the Wii BP scissor's 11-bit fields (+342).
  // Aurora's render-space command preserves the full 32-bit native dimensions.
  GXSetScissorRender(0,0,width,height);
  GXSetCopyClear(GXColor{12,18,38,255},GX_MAX_Z24);
  GXSetCullMode(GX_CULL_NONE);
  GXSetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
  GXSetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
  GXSetColorUpdate(GX_TRUE); GXSetAlphaUpdate(GX_TRUE);
  GXSetNumTexGens(0); GXSetNumChans(1); GXSetNumTevStages(1);
  GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD_NULL,GX_TEXMAP_NULL,GX_COLOR0A0);
  GXSetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
  GXSetChanCtrl(GX_COLOR0A0,GX_FALSE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHT_NULL,GX_DF_NONE,GX_AF_NONE);
  // Orthographic clip coordinates; scale X to keep geometry round after rotation.
  const float aspect=float(height)/float(width);
  const float projection[7]={1,aspect,0,1,0,0,-0.5f};
  GXSetProjectionv(projection);
  float identity[3][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0}};
  GXLoadPosMtxImm(identity,GX_PNMTX0); GXSetCurrentMtx(GX_PNMTX0);
  GXClearVtxDesc();
  GXSetVtxDesc(GX_VA_POS,GX_DIRECT); GXSetVtxDesc(GX_VA_CLR0,GX_DIRECT);
  GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
  GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
  const float angle=frameIndex*0.012f;
  const float vertices[3][2]={{0,0.65f},{-0.56f,-0.325f},{0.56f,-0.325f}};
  const unsigned char colors[3][3]={{255,85,85},{85,255,140},{90,140,255}};
  GXBegin(GX_TRIANGLES,GX_VTXFMT0,3);
  for(unsigned i=0;i<3;++i) {
    float x=vertices[i][0],y=vertices[i][1];
    GXPosition3f32(x*std::cos(angle)-y*std::sin(angle),x*std::sin(angle)+y*std::cos(angle)-0.2f,0);
    GXColor4u8(colors[i][0],colors[i][1],colors[i][2],255);
  }
  GXEnd();
}
