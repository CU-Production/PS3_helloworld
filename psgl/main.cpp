#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <sys/process.h>
#include <sys/spu_initialize.h>
#include <sys/paths.h>
#include <sys/sys_time.h>

#include <PSGL/psgl.h>
#include <PSGL/psglu.h>
#include <Cg/cg.h>

#include <cell/dbgfont.h>
#include <cell/fs/cell_fs_file_api.h>
#include <cell/sysmodule.h>


const bool VSYNC_ON = false; 

GLuint 		gfxWidth = 1280;
GLuint 		gfxHeight = 720;
GLfloat   gfxAspectRatio = 16.0f/9.0f;

// shader binary 
#define VERTEX_PROGRAM_BINARY		  SYS_APP_HOME"/shaders/triangle_fs.cgelf"
#define FRAGMENT_PROGRAM_BINARY		SYS_APP_HOME"/shaders/triangle_vs.cgelf"


CGprogram 		VertexProgram;			//loaded vertex program
CGprogram 		FragmentProgram;		//loaded fragment program  


float vertices[] = {
  -0.5f, -0.5f, 0.0f,
   0.5f, -0.5f, 0.0f,
   0.0f,  0.5f, 0.0f,
};

float colors[] = {
  1.0f, 0.0f, 0.0f,
  0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 1.0f,
};

GLuint PositionVBO;
GLuint ColorVBO;
const int VerticesNum = 3;

CGprogram gfxLoadProgramFromFile(CGprofile target, const char* filename)
{
  CGprogram id = cgCreateProgramFromFile(cgCreateContext(), CG_BINARY, filename, target, NULL, NULL);
  if(!id)
  {
    printf("Failed to load shader program >>%s<<\nExiting\n", filename);
    exit(0);
  }
  else
  return id;
}

static void gfxSysutilCallback(uint64_t status, uint64_t param, void* userdata)
{
	(void) param;
	(void) userdata;

	switch(status) {
	case CELL_SYSUTIL_REQUEST_EXITGAME:
		glFinish();
	  exit(0);
		break;
	case CELL_SYSUTIL_DRAWING_BEGIN:
	case CELL_SYSUTIL_DRAWING_END:
		break;
	default:
		printf( "Graphics common: Unknown status received: 0x%llx\n", status );
	}
}

void gfxRegisterSysutilCallback()
{
	// Register sysutil exit callback
	int ret = cellSysutilRegisterCallback(0, gfxSysutilCallback, NULL);
	if( ret != CELL_OK ) {
		printf( "Registering sysutil callback failed...: error=0x%x\n", ret );
		exit(1);
	}
}

void gfxInitGraphics() 
{
  // Load required prx modules.
  int ret = cellSysmoduleLoadModule(CELL_SYSMODULE_GCM_SYS);
  switch( ret )
  {
      case CELL_OK:
        // The module is successfully loaded,
    break;

      case CELL_SYSMODULE_ERROR_DUPLICATED:
        // The module was already loaded,
    break;

      case CELL_SYSMODULE_ERROR_UNKNOWN:
      case CELL_SYSMODULE_ERROR_FATAL:
    printf("!! Failed to load CELL_SYSMODULE_GCM_SYS\n" ); 
    printf("!! Exiting Program \n" ); 
    exit(1);
  }

  
  ret = cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
  switch( ret )
  {
      case CELL_OK:
        // The module is successfully loaded,
    break;

      case CELL_SYSMODULE_ERROR_DUPLICATED:
        // The module was already loaded,
    break;

      case CELL_SYSMODULE_ERROR_UNKNOWN:
      case CELL_SYSMODULE_ERROR_FATAL:
    printf("!! Failed to load CELL_SYSMODULE_FS\n" ); 
    printf("!! Exiting Program \n" ); 
    exit(1);
  }

  ret = cellSysmoduleLoadModule(CELL_SYSMODULE_USBD);
  switch( ret )
  {
      case CELL_OK:
        // The module is successfully loaded,
    break;

      case CELL_SYSMODULE_ERROR_DUPLICATED:
        // The module was already loaded,
    break;

      case CELL_SYSMODULE_ERROR_UNKNOWN:
      case CELL_SYSMODULE_ERROR_FATAL:
    printf("!! Failed to load CELL_SYSMODULE_USBD\n" ); 
    printf("!! Exiting Program \n" ); 
    exit(1);
  }

  ret = cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
  switch( ret )
  {
      case CELL_OK:
        // The module is successfully loaded,
    break;

      case CELL_SYSMODULE_ERROR_DUPLICATED:
        // The module was already loaded,
    break;

      case CELL_SYSMODULE_ERROR_UNKNOWN:
      case CELL_SYSMODULE_ERROR_FATAL:
    printf("!! Failed to load CELL_SYSMODULE_IO\n" ); 
    printf("!! Exiting Program \n" ); 
    exit(1);
  }

  gfxRegisterSysutilCallback();
  
  // First, initialize PSGL
  // Note that since we initialized the SPUs ourselves earlier we should
  // make sure that PSGL doesn't try to do so as well.
  PSGLinitOptions initOpts={
        enable: PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS | PSGL_INIT_HOST_MEMORY_SIZE,
    maxSPUs: 1,
    initializeSPUs: false,
    // We're not specifying values for these options, the code is only here
    // to alleviate compiler warnings.
    persistentMemorySize: 0,
    transientMemorySize: 0,
    errorConsole: 0,
    fifoSize: 0,	
    hostMemorySize: 128* 1024*1024,  // 128 mbs for host memory 
  };

  psglInit(&initOpts);

  static PSGLdevice* device=NULL;
  //device=psglCreateDeviceAuto(GL_ARGB_SCE,GL_DEPTH_COMPONENT24,GL_MULTISAMPLING_NONE_SCE);
  //device=psglCreateDeviceAuto(GL_ARGB_SCE,GL_DEPTH_COMPONENT24,GL_MULTISAMPLING_2X_DIAGONAL_CENTERED_SCE);
  device=psglCreateDeviceAuto(GL_ARGB_SCE,GL_DEPTH_COMPONENT24,GL_MULTISAMPLING_4X_SQUARE_ROTATED_SCE);
  
  if ( !device )
  {
    printf("!! Failed to init the device \n" ); 
    printf("!! Exiting Program \n" ); 
    exit(1); 
  }
  psglGetDeviceDimensions(device,&gfxWidth,&gfxHeight);

  printf("gfxInitGraphics::PSGL Device Initialized Width %d Height %d \n",gfxWidth, gfxHeight ); 	

  gfxAspectRatio = psglGetDeviceAspectRatio(device);

  // Now create a PSGL context
  PSGLcontext *pContext=psglCreateContext();

  if (pContext==NULL) {
    fprintf(stderr, "Error creating PSGL context\n");
    exit(-1);
  }

  // Make this context current for the device we initialized
  psglMakeCurrent(pContext, device);

  // Since we're using fixed function stuff (i.e. not using our own shader
  // yet), we need to load shaders.bin that contains the fixed function 
  // shaders.
  // psglLoadShaderLibrary( REMOTE_PATH"/shaders.bin");
  psglLoadShaderLibrary( SYS_APP_HOME"/shaders/shaders.bin");

  // Reset the context
  psglResetCurrentContext();
  
  glViewport(0, 0, gfxWidth, gfxHeight);
  glScissor(0, 0, gfxWidth, gfxHeight);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glEnable(GL_DEPTH_TEST);

  // PSGL doesn't clear the screen on startup, so let's do that here.
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  psglSwap();
}

void sampleInit() 
{
  glClearColor(0.1f,0.3f,0.3f, 1.0f);
  glClearDepthf(1.0f);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  glGenBuffers(1,&PositionVBO);
  glBindBuffer(GL_ARRAY_BUFFER,PositionVBO);
  glBufferData(GL_ARRAY_BUFFER,sizeof(GLfloat)*3*VerticesNum,vertices,GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glGenBuffers(1,&ColorVBO);
  glBindBuffer(GL_ARRAY_BUFFER,ColorVBO);
  glBufferData(GL_ARRAY_BUFFER,sizeof(GLfloat)*3*VerticesNum,colors,GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // load the shader programs
  VertexProgram      = gfxLoadProgramFromFile(cgGLGetLatestProfile(CG_GL_VERTEX), VERTEX_PROGRAM_BINARY);
  FragmentProgram    = gfxLoadProgramFromFile(cgGLGetLatestProfile(CG_GL_FRAGMENT), FRAGMENT_PROGRAM_BINARY);

  // bind and enable the vertex and fragment programs
  cgGLBindProgram(VertexProgram);
  cgGLBindProgram(FragmentProgram);
  cgGLEnableProfile(cgGLGetLatestProfile(CG_GL_VERTEX));
  cgGLEnableProfile(cgGLGetLatestProfile(CG_GL_FRAGMENT));

  //FPS reporting (disable vsync to get true time)
  if (VSYNC_ON)
    glEnable(GL_VSYNC_SCE);
  else
    glDisable(GL_VSYNC_SCE);
}
//-----------------------------------------------------------------------------


SYS_PROCESS_PARAM(1001, 0x10000)

int main()
{
  // Initialize 6 SPUs but reserve 1 SPU as a raw SPU for PSGL
  sys_spu_initialize(6, 1);	
  
  // init PSGL and get the current system width and height
  gfxInitGraphics();	
  
  // initialize sample data and projection matrix 
  sampleInit();
  
  while (1)
  {
    // PSGL doesn't clear the screen on startup, so let's do that here.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);		

    // render 
    {
      glEnableClientState(GL_VERTEX_ARRAY);
      glBindBuffer(GL_ARRAY_BUFFER, PositionVBO);
      glVertexPointer(3,GL_FLOAT,0,NULL);

      glEnableClientState(GL_COLOR_ARRAY);
      glBindBuffer(GL_ARRAY_BUFFER, ColorVBO);
      glColorPointer(3,GL_FLOAT,0,NULL);

      glDrawArrays(GL_TRIANGLES,0,VerticesNum);
      
      glBindBuffer(GL_ARRAY_BUFFER, 0 );	 	
      glDisableClientState(GL_COLOR_ARRAY);
      glDisableClientState(GL_VERTEX_ARRAY);

      // glDisable(GL_BLEND);
      // glEnable(GL_DEPTH_TEST);
    }
        
    // swap PSGL buffers 
    psglSwap();
  }
}

