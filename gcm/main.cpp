#define __CELL_ASSERT__
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timer.h>
#include <sys/return_code.h>
#include <sys/process.h>
#include <cell/gcm.h>
#include <cell/sysmodule.h>
#include <stddef.h>
#include <math.h>
#include <sysutil/sysutil_sysparam.h>

#include "gcmutil_error.h"

/* double buffering */
#define COLOR_BUFFER_NUM 2

// For exit routine
static void sysutil_exit_callback(uint64_t status, uint64_t param, void* userdata);
static bool sKeepRunning = true;

using namespace cell::Gcm;

typedef struct
{
  float x, y, z;
  uint32_t rgba; 
} Vertex_t;


/* local memory allocation */
static uint32_t local_mem_heap = 0;
static void *localMemoryAlloc(const uint32_t size) 
{
  uint32_t allocated_size = (size + 1023) & (~1023);
  uint32_t base = local_mem_heap;
  local_mem_heap += allocated_size;
  return (void*)base;
}

static void *localMemoryAlign(const uint32_t alignment, 
    const uint32_t size)
{
  local_mem_heap = (local_mem_heap + alignment-1) & (~(alignment-1));
  return (void*)localMemoryAlloc(size);
}

#define HOST_SIZE (1*1024*1024)


static void setRenderState(void);
static void setDrawEnv(void);


uint32_t display_width;
uint32_t display_height; 

float    display_aspect_ratio;
uint32_t color_pitch;
uint32_t depth_pitch;
uint32_t color_offset[COLOR_BUFFER_NUM];
uint32_t depth_offset;

extern uint32_t _binary_vpshader_vpo_start;
extern uint32_t _binary_vpshader_vpo_end;
extern uint32_t _binary_fpshader_fpo_start;
extern uint32_t _binary_fpshader_fpo_end;

static unsigned char *vertex_program_ptr = 
(unsigned char *)&_binary_vpshader_vpo_start;
static unsigned char *fragment_program_ptr = 
(unsigned char *)&_binary_fpshader_fpo_start;

static CGprogram vertex_program;
static CGprogram fragment_program;

static void *vertex_program_ucode;
static void *fragment_program_ucode;
static uint32_t fragment_offset;
static uint32_t vertex_offset[2];
static uint32_t color_index ;
static uint32_t position_index ;

static uint32_t frame_index = 0;

static void setRenderTarget(const uint32_t Index)
{
  CellGcmSurface sf;
  sf.colorFormat 	= CELL_GCM_SURFACE_A8R8G8B8;
  sf.colorTarget	= CELL_GCM_SURFACE_TARGET_0;
  sf.colorLocation[0]	= CELL_GCM_LOCATION_LOCAL;
  sf.colorOffset[0] 	= color_offset[Index];
  sf.colorPitch[0] 	= color_pitch;

  sf.colorLocation[1]	= CELL_GCM_LOCATION_LOCAL;
  sf.colorLocation[2]	= CELL_GCM_LOCATION_LOCAL;
  sf.colorLocation[3]	= CELL_GCM_LOCATION_LOCAL;
  sf.colorOffset[1] 	= 0;
  sf.colorOffset[2] 	= 0;
  sf.colorOffset[3] 	= 0;
  sf.colorPitch[1]	= 64;
  sf.colorPitch[2]	= 64;
  sf.colorPitch[3]	= 64;

  sf.depthFormat 	= CELL_GCM_SURFACE_Z24S8;
  sf.depthLocation	= CELL_GCM_LOCATION_LOCAL;
  sf.depthOffset	= depth_offset;
  sf.depthPitch 	= depth_pitch;

  sf.type		= CELL_GCM_SURFACE_PITCH;
  sf.antialias	= CELL_GCM_SURFACE_CENTER_1;

  sf.width 		= display_width;
  sf.height 		= display_height;
  sf.x 		= 0;
  sf.y 		= 0;
  cellGcmSetSurface(&sf);
}

/* wait until flip */
static void waitFlip(void)
{
  while (cellGcmGetFlipStatus()!=0){
    sys_timer_usleep(300);
  }
  cellGcmResetFlipStatus();
}


static void flip(void)
{
  static int first=1;

  // wait until the previous flip executed
  if (first!=1) waitFlip();
  else cellGcmResetFlipStatus();

  if(cellGcmSetFlip(frame_index) != CELL_OK) return;
  cellGcmFlush();

  // resend status
  setDrawEnv();
  setRenderState();

  cellGcmSetWaitFlip();

  // New render target
  frame_index = (frame_index+1)%COLOR_BUFFER_NUM;
  setRenderTarget(frame_index);

  first=0;
}


static void initShader(void)
{
  vertex_program   = (CGprogram)vertex_program_ptr;
  fragment_program = (CGprogram)fragment_program_ptr;

  // init
  cellGcmCgInitProgram(vertex_program);
  cellGcmCgInitProgram(fragment_program);

  uint32_t ucode_size;
  void *ucode;
  cellGcmCgGetUCode(fragment_program, &ucode, &ucode_size);
  // 64B alignment required 
  void *ret = localMemoryAlign(64, ucode_size);
  fragment_program_ucode = ret;
  memcpy(fragment_program_ucode, ucode, ucode_size); 

  cellGcmCgGetUCode(vertex_program, &ucode, &ucode_size);
  vertex_program_ucode = ucode;
}


#define CB_SIZE	(0x10000)

static int32_t initDisplay(void)
{
  uint32_t color_depth=4; // ARGB8
  uint32_t z_depth=4;     // COMPONENT24
  void *color_base_addr;
  void *depth_base_addr;
  void *color_addr[COLOR_BUFFER_NUM];
  void *depth_addr;
  CellVideoOutResolution resolution;

  // display initialize

  // read the current video status
  // INITIAL DISPLAY MODE HAS TO BE SET BY RUNNING SETMONITOR.SELF
  CellVideoOutState videoState;
  CELL_GCMUTIL_CHECK_ASSERT(cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &videoState));
  CELL_GCMUTIL_CHECK_ASSERT(cellVideoOutGetResolution(videoState.displayMode.resolutionId, &resolution));

  display_width = resolution.width;
  display_height = resolution.height;
  color_pitch = display_width*color_depth;
  depth_pitch = display_width*z_depth;

  uint32_t color_size =   color_pitch*display_height;
  uint32_t depth_size =  depth_pitch*display_height;

  CellVideoOutConfiguration videocfg;
  memset(&videocfg, 0, sizeof(CellVideoOutConfiguration));
  videocfg.resolutionId = videoState.displayMode.resolutionId;
  videocfg.format = CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8;
  videocfg.pitch = color_pitch;

  // set video out configuration with waitForEvent set to 0 (4th parameter)
  CELL_GCMUTIL_CHECK_ASSERT(cellVideoOutConfigure(CELL_VIDEO_OUT_PRIMARY, &videocfg, NULL, 0));
  CELL_GCMUTIL_CHECK_ASSERT(cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &videoState));

  switch (videoState.displayMode.aspect){
    case CELL_VIDEO_OUT_ASPECT_4_3:
      display_aspect_ratio=4.0f/3.0f;
      break;
    case CELL_VIDEO_OUT_ASPECT_16_9:
      display_aspect_ratio=16.0f/9.0f;
      break;
    default:
      printf("unknown aspect ratio %x\n", videoState.displayMode.aspect);
      display_aspect_ratio=16.0f/9.0f;
  }

  cellGcmSetFlipMode(CELL_GCM_DISPLAY_VSYNC);

  // get config
  CellGcmConfig config;
  cellGcmGetConfiguration(&config);
  // buffer memory allocation
  local_mem_heap = (uint32_t)config.localAddress;

  color_base_addr = localMemoryAlign(64, COLOR_BUFFER_NUM*color_size);

  for (int i = 0; i < COLOR_BUFFER_NUM; i++) {
      color_addr[i]= (void *)((uint32_t)color_base_addr+ (i*color_size));
      CELL_GCMUTIL_CHECK_ASSERT(cellGcmAddressToOffset(color_addr[i], &color_offset[i]));
  }

  // regist surface
  for (int i = 0; i < COLOR_BUFFER_NUM; i++) {
    CELL_GCMUTIL_CHECK_ASSERT(cellGcmSetDisplayBuffer(i, color_offset[i], color_pitch, display_width, display_height));
  }

  depth_base_addr = localMemoryAlign(64, depth_size);
  depth_addr = depth_base_addr;
  CELL_GCMUTIL_CHECK_ASSERT(cellGcmAddressToOffset(depth_addr, &depth_offset));

  return 0;
}

static void setVertex(Vertex_t* vertex_buffer)
{
  vertex_buffer[0].x = -0.5f; 
  vertex_buffer[0].y = -0.5f; 
  vertex_buffer[0].z =  0.0f; 
  vertex_buffer[0].rgba=0x00ff0000;

  vertex_buffer[1].x =  0.5f; 
  vertex_buffer[1].y = -0.5f; 
  vertex_buffer[1].z =  0.0f; 
  vertex_buffer[1].rgba=0x0000ff00;

  vertex_buffer[2].x =  0.0f; 
  vertex_buffer[2].y =  0.5f; 
  vertex_buffer[2].z =  0.0f; 
  vertex_buffer[2].rgba=0xff000000;
}

static void setDrawEnv(void)
{
  cellGcmSetColorMask(CELL_GCM_COLOR_MASK_B|
      CELL_GCM_COLOR_MASK_G|
      CELL_GCM_COLOR_MASK_R|
      CELL_GCM_COLOR_MASK_A);

  cellGcmSetColorMaskMrt(0);
  uint16_t x,y,w,h;
  float min, max;
  float scale[4],offset[4];

  x = 0;
  y = 0;
  w = display_width;
  h = display_height;
  min = 0.0f;
  max = 1.0f;
  scale[0] = w * 0.5f;
  scale[1] = h * -0.5f;
  scale[2] = (max - min) * 0.5f;
  scale[3] = 0.0f;
  offset[0] = x + scale[0];
  offset[1] = y + h * 0.5f;
  offset[2] = (max + min) * 0.5f;
  offset[3] = 0.0f;

  cellGcmSetViewport(x, y, w, h, min, max, scale, offset);
  cellGcmSetClearColor((64<<0)|(64<<8)|(64<<16)|(64<<24));

  cellGcmSetDepthTestEnable(CELL_GCM_TRUE);
  cellGcmSetDepthFunc(CELL_GCM_LESS);

}

static void setRenderState(void)
{
  cellGcmSetVertexProgram(vertex_program, vertex_program_ucode);
  cellGcmSetVertexDataArray(position_index,
      0, 
      sizeof(Vertex_t), 
      3, 
      CELL_GCM_VERTEX_F, 
      CELL_GCM_LOCATION_LOCAL, 
      (uint32_t)vertex_offset[0]);
  cellGcmSetVertexDataArray(color_index,
      0, 
      sizeof(Vertex_t), 
      4, 
      CELL_GCM_VERTEX_UB, 
      CELL_GCM_LOCATION_LOCAL, 
      (uint32_t)vertex_offset[1]);



  cellGcmSetFragmentProgram(fragment_program, fragment_offset);

}

static int32_t setRenderObject(void)
{
  static Vertex_t *vertex_buffer;
  void *ret = localMemoryAlign(128, sizeof(Vertex_t) * 3);
  vertex_buffer = (Vertex_t*)ret;
  setVertex(vertex_buffer);

  CGparameter position = cellGcmCgGetNamedParameter(vertex_program, "position");
  CELL_GCMUTIL_CG_PARAMETER_CHECK_ASSERT(position);
  CGparameter color = cellGcmCgGetNamedParameter(vertex_program, "color");
  CELL_GCMUTIL_CG_PARAMETER_CHECK_ASSERT(color);

  // get Vertex Attribute index
  position_index = cellGcmCgGetParameterResource(vertex_program, position) - CG_ATTR0;
  color_index = cellGcmCgGetParameterResource(vertex_program, color) - CG_ATTR0;

  // fragment program offset
  CELL_GCMUTIL_CHECK_ASSERT(cellGcmAddressToOffset(fragment_program_ucode, &fragment_offset));
  CELL_GCMUTIL_CHECK_ASSERT(cellGcmAddressToOffset(&vertex_buffer->x, &vertex_offset[0]));
  CELL_GCMUTIL_CHECK_ASSERT(cellGcmAddressToOffset(&vertex_buffer->rgba, &vertex_offset[1]));
  
  return 0;
}

SYS_PROCESS_PARAM(1001, 0x10000);
int main(void)
{
  void* host_addr = memalign(1024*1024, HOST_SIZE);
  CELL_GCMUTIL_ASSERTS(host_addr != NULL,"memalign()");
  CELL_GCMUTIL_CHECK_ASSERT(cellGcmInit(CB_SIZE, HOST_SIZE, host_addr));

  if (initDisplay()!=0)	return -1;

  initShader();

  setDrawEnv();

  if (setRenderObject())
    return -1;

  setRenderState();

  // 1st time
  setRenderTarget(frame_index);

  // Exit routines
  {
    // register sysutil exit callback
    CELL_GCMUTIL_CHECK_ASSERT(cellSysutilRegisterCallback(0, sysutil_exit_callback, NULL));
  }

  // rendering loop
  CELL_GCMUTIL_ASSERT(sKeepRunning);
  while (sKeepRunning) {
    // check system event
    CELL_GCMUTIL_CHECK_ASSERT(cellSysutilCheckCallback());

    // clear frame buffer
    cellGcmSetClearSurface(CELL_GCM_CLEAR_Z|
        CELL_GCM_CLEAR_R|
        CELL_GCM_CLEAR_G|
        CELL_GCM_CLEAR_B|
        CELL_GCM_CLEAR_A);
    // set draw command
    cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_TRIANGLES, 0, 3);

    // start reading the command buffer
    flip();		

  }
  
  // Let RSX wait for final flip
  cellGcmSetWaitFlip();

  // Let PPU wait for all commands done (include waitFlip)
  cellGcmFinish(1);

  return 0;
}

void sysutil_exit_callback(uint64_t status, uint64_t param, void* userdata)
{
  (void) param;
  (void) userdata;

  switch(status) {
  case CELL_SYSUTIL_REQUEST_EXITGAME:
    sKeepRunning = false;
    break;
  case CELL_SYSUTIL_DRAWING_BEGIN:
  case CELL_SYSUTIL_DRAWING_END:
  case CELL_SYSUTIL_SYSTEM_MENU_OPEN:
  case CELL_SYSUTIL_SYSTEM_MENU_CLOSE:
  case CELL_SYSUTIL_BGMPLAYBACK_PLAY:
  case CELL_SYSUTIL_BGMPLAYBACK_STOP:
  default:
    break;
  }
}


