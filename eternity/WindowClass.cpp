////////////////////////////////////////////////////
// Windows "plumbing".                            //
// You can safely ignore eveorything in this file. //
////////////////////////////////////////////////////

#include "WindowClass.h"

// A class that "abstracts away" the process
// of getting a window up on the screen.
Window::Window( HINSTANCE hInst, TCHAR* windowTitleBar, int windowXPos, int windowYPos, int windowWidth, int windowHeight )
{
  // Save off these parameters in private instance variables.
  hInstance = hInst ;
  d3dpps.BackBufferWidth = windowWidth ;
  d3dpps.BackBufferHeight = windowHeight ;

  // Create a window.
  WNDCLASSEX window = { 0 } ;
  window.cbSize			= sizeof( WNDCLASSEX ) ;
  window.hbrBackground	= (HBRUSH)GetStockObject( WHITE_BRUSH );
  window.hCursor = LoadCursor( NULL, IDC_ARROW ) ;
  window.hIcon = LoadIcon( NULL, IDI_APPLICATION ) ;
  window.hIconSm = LoadIcon( NULL, IDI_APPLICATION ) ;
  window.hInstance = hInstance ;
  window.lpfnWndProc = WndProc ;
  window.lpszClassName = TEXT( "myWindow" ) ;  // XXXX MATCH ME XXXX
  window.lpszMenuName = NULL;
  window.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC ;

  if(!RegisterClassEx( &window ))
  {
    bail( "Something's wrong with the WNDCLASSEX structure you defined.. quitting" ) ;
    return ;
  }

  RECT wndSize ;
  wndSize.left = windowXPos ;
  wndSize.right = windowXPos + windowWidth ;
  wndSize.top = windowYPos ;
  wndSize.bottom = windowYPos + windowHeight ;

  AdjustWindowRectEx( &wndSize, WS_OVERLAPPEDWINDOW, NULL, 0 ) ;

  // Create the main window
  hwnd = CreateWindowEx(

    0 /*WS_EX_TOPMOST*/,  // extended window style.. if set to WS_EX_TOPMOST, for example,
    // then your window will be the topmost window all the time.  Setting it to 0
    // will make your window just another regular everyday window (not topmost or anything).

    TEXT( "myWindow" ),  // window class name.. must match XXXX MATCH ME XXXX above, exactly

    windowTitleBar,      // text in title bar of your window

    WS_OVERLAPPEDWINDOW, // window style.  Try using WS_POPUP, for example.

    wndSize.left, wndSize.top,
    wndSize.right - wndSize.left, wndSize.bottom - wndSize.top,

    // Don't worry about the next 4, not important for now.
    NULL, NULL,
    hInstance, NULL
  ) ;
  ShowWindow( hwnd, SW_SHOWNORMAL ) ;
  UpdateWindow( hwnd ) ;

  info( "Starting up Direct3D..." ) ;
  if( !initD3D() )
  {
    bail( "D3D failed to initialize. Check your set up params in initD3D() function, check your width and height are valid" ) ;
  }
  initDefaultSprite() ;
  initWhitePixel() ;

  info( "Starting up rawinput devices..." ) ;
  startupRawInput();

  info( "Turning off the mouse cursor..." ) ;
  ShowCursor( FALSE ) ;

  // start up fmod
  initFMOD() ;

  paused = false ;

}

Window::~Window()
{
  // ... clean up and shut down ... 

  // D3D
  foreach( SpriteMapIter, iter, sprites )
  {
    delete iter->second ;
  }

  SAFE_RELEASE( gpu ) ;
  SAFE_RELEASE( d3d ) ;

  
}


bool Window::initD3D()
{
  // start by nulling out both pointers:
  d3d = 0 ;
  gpu = 0 ;

  d3d = Direct3DCreate9( D3D_SDK_VERSION ) ;

  if( d3d == NULL )
  {
    error( "D3DDevice creation FAILED" ) ;
    return false ;
  }

  info( "Direct3D9 device created successfully" ) ;


  memset( &d3dpps, 0, sizeof( D3DPRESENT_PARAMETERS ) ) ;

  d3dpps.BackBufferCount = 1 ;
  d3dpps.SwapEffect = D3DSWAPEFFECT_DISCARD  ;
  d3dpps.BackBufferFormat = D3DFMT_UNKNOWN ;
  d3dpps.EnableAutoDepthStencil = true ;
  d3dpps.AutoDepthStencilFormat = D3DFMT_D16 ;
  d3dpps.hDeviceWindow = hwnd ;
  
  d3dpps.BackBufferFormat = D3DFMT_X8R8G8B8 ;
  
  d3dpps.Windowed = true ;
  //d3dpps.BackBufferWidth =  ;  // these have already been set
  //d3dpps.BackBufferHeight =  ; // in Window() ctor
  //d3dpps.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE ; // FRAMERATE::UNBOUNDED  You need to
  // uncomment this line to make the GPU flip over really really fast

  HRESULT hr = d3d->CreateDevice(

    D3DADAPTER_DEFAULT, // primary display adapter
    D3DDEVTYPE_HAL,     // use HARDWARE rendering
    hwnd,
    D3DCREATE_HARDWARE_VERTEXPROCESSING,
    &d3dpps,
    &gpu

  ) ;


  if( !DX_CHECK( hr, "initialize Direct3D" ) )
  {
    error( "Init d3d FAILED" ) ;
    return false ;
  }


  info( "Direct3D9 GPU device creation successful" ) ;

  isDeviceLost = false ;

  gpu->GetDeviceCaps( &caps ) ;
  


  hr = gpu->SetFVF( D3DFVF_XYZ | D3DFVF_DIFFUSE | (1 << D3DFVF_TEXCOUNT_SHIFT) ) ;
  DX_CHECK( hr, "SetFVF" ) ;

  int cumulativeOffset = 0 ;
  D3DVERTEXELEMENT9 pos ;
  pos.Usage = D3DDECLUSAGE_POSITION ;
  pos.UsageIndex = 0 ;
  pos.Stream = 0 ;
  pos.Type = D3DDECLTYPE_FLOAT3 ;
  pos.Offset = cumulativeOffset ;
  pos.Method = D3DDECLMETHOD_DEFAULT ; 
  cumulativeOffset += 3*sizeof(float) ;

  D3DVERTEXELEMENT9 col;
  col.Usage = D3DDECLUSAGE_COLOR ;
  col.UsageIndex = 0 ;
  col.Stream = 0 ;
  col.Type = D3DDECLTYPE_D3DCOLOR ;
  col.Offset = cumulativeOffset ;
  col.Method = D3DDECLMETHOD_DEFAULT ;
  cumulativeOffset += sizeof( D3DCOLOR ) ;

  D3DVERTEXELEMENT9 uv ;
  uv.Usage = D3DDECLUSAGE_TEXCOORD ;
  uv.UsageIndex = 0 ;
  uv.Stream = 0 ;
  uv.Type = D3DDECLTYPE_FLOAT2 ;
  uv.Offset = cumulativeOffset ;
  uv.Method = D3DDECLMETHOD_DEFAULT ;
  cumulativeOffset += 2*sizeof(float) ;

  D3DVERTEXELEMENT9 vertexElements[] =
  {
    pos,
    col,
    uv,

    D3DDECL_END()
  } ;

  IDirect3DVertexDeclaration9 * Vdecl ;

  hr = gpu->CreateVertexDeclaration( vertexElements, &Vdecl ) ;
  DX_CHECK( hr, "CreateVertexDeclaration" ) ;

  hr = gpu->SetVertexDeclaration( Vdecl ) ;
  DX_CHECK( hr, "SetVertexDeclaration" ) ;

  hr = gpu->SetRenderState( D3DRS_COLORVERTEX, TRUE ) ;
  DX_CHECK( hr, "SetRenderState( COLORVERTEX )" ) ;

  hr = gpu->SetRenderState( D3DRS_LIGHTING, FALSE ) ;
  DX_CHECK( hr, "Lighting off" ) ;

  int sampleType = D3DTEXF_LINEAR ;  // change to D3DTEXF_NONE to
  // make images BLOCKY and stop the "blurring"

  // D3DTEXF_LINEAR:  Blur the color between pixels nicely
  // D3DTEXF_NONE:  BLOCKY PIXELS.


  hr = gpu->SetSamplerState( 0, D3DSAMP_MAGFILTER, sampleType ) ;
  DX_CHECK( hr, "mag filter" ) ;

  // Set minification filter, 
  hr = gpu->SetSamplerState( 0, D3DSAMP_MINFILTER, sampleType ) ;
  DX_CHECK( hr, "min filter" ) ;

  // Enable mipmapping in general
  hr = gpu->SetSamplerState( 0, D3DSAMP_MIPFILTER, sampleType ) ;
  DX_CHECK( hr, "mip filter" ) ;

  hr = gpu->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE ) ;
  DX_CHECK( hr, "cull mode off" ) ;

  hr = gpu->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE ) ;
  DX_CHECK( hr, "alpha blending on" ) ;


  
  D3DXCreateFontA( gpu, 18, 0, FW_BOLD, 1,
    FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY,
    DEFAULT_PITCH | FF_DONTCARE, "Arial", &id3dxDefaultFont ) ;

  D3DXCreateSprite( gpu, &id3dxSpriteRenderer ) ;

  // start the mouse in the middle of the screen
  mouse.setX( getWidth() / 2 ) ;
  mouse.setY( getHeight() / 2 ) ;

  // set the clipzone to match initialized window size
  RECT clipZone = { 0, 0, getWidth(), getHeight() } ;
  mouse.setClipZone( clipZone ) ;


  clearColor = D3DCOLOR_ARGB( 255, 0, 10, 45 ) ;

  return true ;
}















// Generates you a texture with colored
// text on a clear background
void Window::boxedTextSprite( int spriteId, char *str, D3DCOLOR textColor )
{
  RECT rect = { 0 } ;
  boxedTextSprite( spriteId, str, textColor, 0, rect, id3dxDefaultFont ) ;
}

void Window::boxedTextSprite( int spriteId, char *str, D3DCOLOR textColor, D3DCOLOR backgroundColor )
{
  RECT rect = { 0 } ;
  boxedTextSprite( spriteId, str, textColor, backgroundColor, rect, id3dxDefaultFont ) ;
}

void Window::boxedTextSprite( int spriteId, char *str, D3DCOLOR textColor, D3DCOLOR backgroundColor, int padding )
{
  RECT rect = { padding, padding, padding, padding } ;
  boxedTextSprite( spriteId, str, textColor, backgroundColor, rect, id3dxDefaultFont ) ;
}

void Window::boxedTextSprite( int spriteId, char *str, D3DCOLOR textColor, D3DCOLOR backgroundColor, RECT padding )
{
  boxedTextSprite( spriteId, str, textColor, backgroundColor, padding, id3dxDefaultFont ) ;
}

void Window::boxedTextSprite( int spriteId, char *str, D3DCOLOR textColor, D3DCOLOR backgroundColor, RECT padding, char *fontName, float size, int boldness, bool italics )
{
  ID3DXFont *id3dxFont ;

  D3DXCreateFontA( gpu, size, 0, boldness, 1, italics,
    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY,
    DEFAULT_PITCH | FF_DONTCARE, fontName, &id3dxFont ) ;

  boxedTextSprite( spriteId, str, textColor, backgroundColor, padding, id3dxFont ) ;
  
  SAFE_RELEASE( id3dxFont ) ;
}

//private:
void Window::boxedTextSprite( int spriteId, char *str, D3DCOLOR textColor, D3DCOLOR backgroundColor, RECT padding, ID3DXFont *font ) 
{
  #pragma region compute the limiting rect
  RECT computedRect = { 0, 0, 1, 1 } ;
  
  int height = font->DrawTextA(
    id3dxSpriteRenderer, str, -1,
    &computedRect,
    DT_CALCRECT /* | dtOptions */, 0 ) ;

  info( "computed rect %d %d %d %d, height %d",
    computedRect.left, computedRect.top,
    computedRect.bottom, computedRect.right, height ) ;

  //computedRect.bottom += height ;

  RECT boxRect = computedRect ;

  boxRect.right += padding.left + padding.right ;
  boxRect.bottom += padding.top + padding.bottom ;

  RECT textRect = boxRect ;
  textRect.left += padding.left ;
  textRect.right -= padding.right ;
  textRect.top += padding.top ;
  textRect.bottom -= padding.bottom ;
  #pragma endregion

  // Create the render target surface to be the right
  // size for the text that is going to be drawn.
  #pragma region create the render target and pixel patch
  HRESULT hr ;
  IDirect3DTexture9 *renderTargetTexture ;
  int w = boxRect.right - boxRect.left ;
  int h = boxRect.bottom - boxRect.top ;
  
  hr = D3DXCreateTexture( gpu, w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &renderTargetTexture ) ;
  DX_CHECK( hr, "Create render target texture" ) ;

  IDirect3DTexture9 *pixelPatch4x4 ;
  
  hr = D3DXCreateTexture( gpu, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pixelPatch4x4 ) ;
  DX_CHECK( hr, "Create 4x4 pixel patch" ) ;
  #pragma endregion

  // Lock the pixelpatch's rect and write the color data.
  #pragma region lock the rect
  D3DLOCKED_RECT lockedRect ;
  DX_CHECK( pixelPatch4x4->LockRect( 0, &lockedRect, NULL, 0 ), "Lock 4x4 pixel patch rect" ) ;
  __int32 *pData = reinterpret_cast<__int32*>( lockedRect.pBits ) ;  // 32 bit ints
  for( int i = 0 ; i < 4*4 ; i++ )
    pData[ i ] = backgroundColor ;
  DX_CHECK( pixelPatch4x4->UnlockRect( 0 ), "Unlock 4x4 pixel patch" ) ;
  #pragma endregion

  // Now draw out a rectangle sprite
  // using that pixel patch, stretching it
  #pragma region set up render target
  IDirect3DSurface9 *rtSurface, *screenSurface ;
  DX_CHECK( renderTargetTexture->GetSurfaceLevel( 0, &rtSurface ), "Get surface for render target" ) ;
  
  // Get the old surface target
  DX_CHECK( gpu->GetRenderTarget( 0, &screenSurface ), "Get old render target" ) ;
  DX_CHECK( gpu->SetRenderTarget( 0, rtSurface ), "Set new render target" ) ;
  #pragma endregion

  // Draw out the box shape and the text to that box.
  if( !DX_CHECK( gpu->BeginScene(), "Begin scene on RT" ) )
  {
    error( "Are you trying to call boxedTextSprite() in the Draw() function!!  "
      "You should call it in the Init() function, generate the sprite, and save off "
      "the id." ) ;
    return ;
  }
  id3dxSpriteRenderer->Begin( D3DXSPRITE_ALPHABLEND ) ;
  
  // Because its a 4x4 pixel patch.
  D3DXVECTOR2 vec2Scale( w/4.0f, h/4.0f ) ;
  
  D3DXMATRIX tMatrix ;
  
  D3DXMatrixTransformation2D(
    &tMatrix,
    NULL,  // leave it in the top left corner
    0.0f,  // wacky warp behavior -- don't use
    &vec2Scale, // how much to stretch the pixelPatch
    NULL,  // rotate from top left corner..
    0,     // rotation angle is always 0:  you shouldn't be rotating this.  you should
           // be rotating the sprite AFTER its been produced and you're using it to draw.
    NULL   // translation:  start in top left corner
  ) ;
  
  id3dxSpriteRenderer->SetTransform( &tMatrix ) ;
  id3dxSpriteRenderer->Draw( pixelPatch4x4, NULL, NULL, NULL, D3DCOLOR_ARGB(255,255,255,255) ) ;

  D3DXMatrixIdentity( &tMatrix ) ;
  id3dxSpriteRenderer->SetTransform( &tMatrix ) ;
  font->DrawTextA( id3dxSpriteRenderer, str, -1, &textRect, DT_CENTER | DT_VCENTER, textColor ) ;
  
  id3dxSpriteRenderer->End() ;
  DX_CHECK( gpu->EndScene(), "End scene on RT" ) ;

  DX_CHECK( gpu->SetRenderTarget( 0, screenSurface ), "Set render target back to screen surface" ) ;
  


  // register the sprite
  Sprite *sprite = new Sprite( w, h, renderTargetTexture ) ;
  addSprite( spriteId, sprite ) ;

  info( "Successfully created your boxed text sprite, id=%d", spriteId ) ;

  // Destroy that 4x4 pixel patch
  SAFE_RELEASE( pixelPatch4x4 ) ;
}

void Window::drawSprite( int id, float x, float y )
{
  drawSprite( id, x, y, SPRITE_READ_FROM_FILE, SPRITE_READ_FROM_FILE, 0.0f, D3DCOLOR_ARGB( 255, 255, 255, 255 ), SpriteCentering::Center ) ;
}

void Window::drawSprite( int id, float x, float y, SpriteCentering centering )
{
  drawSprite( id, x, y, SPRITE_READ_FROM_FILE, SPRITE_READ_FROM_FILE, 0.0f, D3DCOLOR_ARGB( 255, 255, 255, 255 ), centering ) ;
}

void Window::drawSprite( int id, float x, float y, D3DCOLOR modulatingColor )
{
  drawSprite( id, x, y, SPRITE_READ_FROM_FILE, SPRITE_READ_FROM_FILE, 0.0f, modulatingColor, SpriteCentering::Center ) ;
}

void Window::drawSprite( int id, float x, float y, float width, float height )
{
  drawSprite( id, x, y, width, height, 0.0f, D3DCOLOR_ARGB( 255, 255, 255, 255 ), SpriteCentering::Center ) ;
}

void Window::drawSprite( int id, float x, float y, float width, float height, SpriteCentering centering )
{
  drawSprite( id, x, y, width, height, 0.0f, D3DCOLOR_ARGB( 255, 255, 255, 255 ), centering ) ;
}

void Window::drawSprite( int id, float x, float y, float width, float height, float angle )
{
  drawSprite( id, x, y, width, height, angle, D3DCOLOR_ARGB( 255, 255, 255, 255 ), SpriteCentering::Center ) ;
}

void Window::drawSprite( int id, float x, float y, float width, float height, float angle, D3DCOLOR modulatingColor )
{
  drawSprite( id, x, y, width, height, angle, modulatingColor, SpriteCentering::Center ) ;
}
void Window::drawSprite( int id, float x, float y, float width, float height, float angle, D3DCOLOR modulatingColor, SpriteCentering centering )
{
  Sprite *sprite = defaultSprite ;
  SpriteMapIter spriteEntry = sprites.find( id ) ;

  if( spriteEntry != sprites.end() )
    sprite = spriteEntry->second ;
  else
    warning( "Sprite %d not loaded, using default sprite instead", id ) ;
  
  //!!! BUGS for -1 scale.  this won't work.
  // no sentinel value.  use separate function
  // for scaling a sprite
  if( width == SPRITE_READ_FROM_FILE )
    width = sprite->getSpriteWidth() ;
  if( height == SPRITE_READ_FROM_FILE )
    height = sprite->getSpriteHeight() ;

  float spriteWidth = sprite->getSpriteWidth() ;
  float spriteHeight = sprite->getSpriteHeight() ;

  float scaleX = width / spriteWidth ;
  float scaleY = height / spriteHeight ;

  //D3DXVECTOR2 vec2Scale( sprite->getScaleXFor( width ), sprite->getScaleYFor( height ) ) ;
  D3DXVECTOR2 vec2Scale( scaleX, scaleY ) ;
  D3DXVECTOR2 vec2Trans( x, y ) ;

  //printf( "Scale %f %f\n", vec2Scale.x, vec2Scale.y ) ;
  
  D3DXVECTOR3 vec3Center( 0.0f, 0.0f, 0.0f ) ;
  if( centering == SpriteCentering::Center )
  {
    vec3Center.x = sprite->getCenterX() ;
    vec3Center.y = sprite->getCenterY() ;
  }
  // (otherwise its 0, for top left corner "centering")

  //D3DXVECTOR3 vec3Pos( x, y, 0 ) ; ; // do not use this.

  D3DXMATRIX matrix ;
  //D3DXMatrixIdentity( &matrix ) ;
  D3DXMatrixTransformation2D( &matrix, NULL, 0, &vec2Scale, NULL, angle, &vec2Trans ) ;

  //!! performance.  All this starting and stopping
  // the sprite rendered is going to hurt performance.
  // Really should batch these.
  id3dxSpriteRenderer->Begin( D3DXSPRITE_ALPHABLEND ) ;
  id3dxSpriteRenderer->SetTransform( &matrix ) ;

  RECT rect = sprite->getRect() ;
  id3dxSpriteRenderer->Draw(
    
    sprite->getTexture(),
    &rect,
    &vec3Center,   // The center of the sprite.  (0,0) is the upper left hand corner. OF THE SPRITE.
    NULL,      // the position of the sprite, (assuming you haven't translated it using a matrix already, which we DID do here!)
    modulatingColor
  
  ) ;
  id3dxSpriteRenderer->End();
}

//!! this really needs to be in a spriteManager so we can adequately
// protect the sprites map from collision
void Window::addSprite( int id, Sprite *sprite )
{
  SpriteMapIter existingSpritePtr = sprites.find( id ) ;
  if( existingSpritePtr != sprites.end() )
  {
    // the sprite with that id existed.  erase it
    warning( "Sprite with id=%d already existed.  Destroying it..", id ) ;
    delete existingSpritePtr->second ;
  }

  // now add it
  sprites.insert( make_pair( id, sprite ) ) ;
}



void Window::loadSprite( int id, char *filename )
{
  loadSprite( id, filename, D3DCOLOR_ARGB( 0,0,0,0 ),
    SPRITE_READ_FROM_FILE, SPRITE_READ_FROM_FILE,
    SPRITE_READ_FROM_FILE, 0.5f ) ;
}

void Window::loadSprite( int id, char *filename, D3DCOLOR backgroundColor )
{
  loadSprite( id, filename, backgroundColor,
    SPRITE_READ_FROM_FILE, SPRITE_READ_FROM_FILE,
    SPRITE_READ_FROM_FILE, 0.5f ) ;
}

// id is how you will refer to this sprite after its been loaded
// filename is just the filename on the disk drive
void Window::loadSprite( int id, char *filename,
                         D3DCOLOR backgroundColor,
                         int singleSpriteWidth, int singleSpriteHeight,
                         int numFrames, float timeBetweenFrames )
{
  Sprite *sprite = new Sprite( gpu, filename, backgroundColor,
    singleSpriteWidth, singleSpriteHeight, numFrames, timeBetweenFrames ) ;
  addSprite( id, sprite ) ;
}

int Window::randomSpriteId()
{
  SpriteMapIter iter = sprites.begin() ;
  advance( iter, rand()%sprites.size() ) ;

  return iter->first ;
}


// Try 3 times.  if you miss 3 times,
// (x--then proceed forward through from low to high
// if that fails--x), tell them with a warning,
// and just return the first item in the map
int Window::randomSpriteId( int below )
{
  SpriteMapIter iter ;

  int numSprites = sprites.size() ;
  if( numSprites < below )
  {
    //warning( "There aren't %d sprites", below ) ;
    below = numSprites ; // better chance of
    // getting # below if use this lower range..
  }

  for( int i = 0 ; i < 3 ; i++ )
  {
    iter = sprites.begin() ;
    int advAmt = rand()%below ;
    advance( iter, advAmt ) ;

    if( iter->first < below )
      return iter->first ;
  }
  
  warning( "I tried 3 times and couldn't find you a sprite lower than %d, returning first", below ) ;
  return sprites.begin()->first ;
}

void Window::drawAxes()
{
  
  static D3DVertex axis[] = {

    // x-axis is red
    D3DVertex( -getWidth(), 0, 0, 0, 0, 255, 0, 0 ),
    D3DVertex( +getWidth(), 0, 0, 0, 0, 255, 0, 0 ),

    // y-axis green
    D3DVertex( 0, -getHeight(), 0, 0, 0, 0, 255, 0 ),
    D3DVertex( 0, +getHeight(), 0, 0, 0, 0, 255, 0 ),

    // z-axis blue
    D3DVertex( 0, 0, -getHeight(), 0, 0, 0, 0, 255 ),
    D3DVertex( 0, 0, +getHeight(), 0, 0, 0, 0, 255 )

  } ;


  HRESULT hr = gpu->SetTexture( 0, NULL ) ;
  DX_CHECK( hr, "unset the SetTexture" ) ;
  
  hr = gpu->DrawPrimitiveUP( D3DPT_LINELIST, 3, axis, sizeof( D3DVertex ) ) ;
  DX_CHECK( hr, "DrawPrimitiveUP FAILED!" ) ;

  static float pointSize = 8.0f ;

  gpu->SetRenderState( D3DRS_POINTSIZE, CAST_AS_DWORD( pointSize ) ) ;

  // Draw points at end of axis.
  static D3DVertex points[] = {
    D3DVertex( getWidth(), 0, 0, 0, 0, 255, 0, 0 ),
    D3DVertex( 0, getHeight(), 0, 0, 0, 0, 255, 0 ),
    D3DVertex( 0, 0, getHeight(), 0, 0, 0, 0, 255 ),
  } ;

  hr = gpu->DrawPrimitiveUP( D3DPT_POINTLIST, 3, points, sizeof( D3DVertex ) ) ;
  DX_CHECK( hr, "DrawPrimitiveUP FAILED!" ) ;
}

bool Window::beginDrawing()
{
  HRESULT hr ;

  if( isDeviceLost )
  {
    //warning( "The d3d device is lost right now, not beginning to draw" ) ;
    return false ;
  }

  hr = gpu->Clear( 0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
    clearColor, 1.0f, 0 ) ;
  DX_CHECK( hr, "Clear error" ) ;

  #pragma region set up the camera
  D3DXMATRIX projx ;

  D3DXMatrixOrthoRH( &projx, getWidth(), getHeight(), 1, 100 ) ;
  //D3DXMatrixPerspectiveFovRH( &projx, PI/4, backBufferWidth/backBufferHeight, 1.0f, 1000.0f ) ;

  gpu->SetTransform( D3DTS_PROJECTION, &projx ) ;

  D3DXMATRIX viewx ;

  int x = getWidth() / 2 ;
  int y = getHeight() / 2 ;
  D3DXVECTOR3 eye( x, y, 10 ) ;
  D3DXVECTOR3 look( x, y, 0 ) ;
  D3DXVECTOR3 up( 0, 1, 0 ) ;
  D3DXMatrixLookAtRH( &viewx, &eye, &look, &up ) ;
  gpu->SetTransform( D3DTS_VIEW, &viewx ) ;
  #pragma endregion

  hr = gpu->BeginScene() ;
  DX_CHECK( hr, "BeginScene error" ) ;

  return true ;
}

void Window::endDrawing()
{
  HRESULT hr ;

  if( isDeviceLost )
  {
    //warning( "The d3d device is lost right now, not ending the draw" ) ;
    return ;
  }

  hr = gpu->EndScene() ;
  DX_CHECK( hr, "EndScene FAILED!" ) ;

  // And finally, PRESENT what we drew to the backbuffer
  hr = gpu->Present( 0, 0, 0, 0 ) ;
  DX_CHECK( hr, "Present FAILED!" ) ;
}

void Window::drawBox( D3DCOLOR color, RECT &r )
{
  drawBox( color, r.left, r.top, r.right - r.left, r.bottom - r.top ) ;
}

void Window::drawBox( D3DCOLOR color, int xLeft, int yTop, int width, int height )
{
  D3DXVECTOR2 vec2Pos( xLeft, yTop ) ;
  D3DXVECTOR2 vec2Scale( width, height ) ;
  
  D3DXMATRIX tMatrix ;
  
  D3DXMatrixTransformation2D(
    &tMatrix,
    NULL,  // leave it in the top left corner
    0.0f,  // wacky warp behavior -- don't use
    &vec2Scale, // how much to stretch the pixelPatch
    NULL,  // rotate from top left corner..
    0,     // rotation angle is always 0:  you shouldn't be rotating this.  you should
           // be rotating the sprite AFTER its been produced and you're using it to draw.
    &vec2Pos   // translation:  relative to top left corner
  ) ;
  
  DX_CHECK( id3dxSpriteRenderer->Begin( D3DXSPRITE_ALPHABLEND ), "Begin draw box, sprite" ) ;
  DX_CHECK( id3dxSpriteRenderer->SetTransform( &tMatrix ), "Set matrix" ) ;
  DX_CHECK( id3dxSpriteRenderer->Draw( whitePixel->getTexture(), NULL, NULL, NULL, color ), "Draw box" ) ;
  DX_CHECK( id3dxSpriteRenderer->End(), "End draw box, sprite" ) ;
}

void Window::drawBoxCentered( D3DCOLOR color, int xCenter, int yCenter, int width, int height )
{
  D3DXVECTOR2 vec2Pos( xCenter - width/2, yCenter - height/2 ) ;
  D3DXVECTOR2 vec2Scale( width, height ) ;

  D3DXMATRIX tMatrix ;
  
  D3DXMatrixTransformation2D(
    &tMatrix,
    NULL,  // leave it in the top left corner
    0.0f,  // wacky warp behavior -- don't use
    &vec2Scale, // how much to stretch the pixelPatch
    NULL,  // rotate from top left corner..
    0,     // rotation angle is always 0:  you shouldn't be rotating this.  you should
           // be rotating the sprite AFTER its been produced and you're using it to draw.
    &vec2Pos   // translation:  relative to top left corner
  ) ;
  
  DX_CHECK( id3dxSpriteRenderer->Begin( D3DXSPRITE_ALPHABLEND ), "Begin draw box, sprite" ) ;
  DX_CHECK( id3dxSpriteRenderer->SetTransform( &tMatrix ), "Set matrix" ) ;
  DX_CHECK( id3dxSpriteRenderer->Draw( whitePixel->getTexture(), NULL, NULL, NULL, color ), "Draw box centered" ) ;
  DX_CHECK( id3dxSpriteRenderer->End(), "End draw box, sprite" ) ;
}

void Window::getBoxDimensions( char *str, RECT &r )
{
  SetRect( &r, 0, 0, 1, 1 ) ;
  int height = id3dxDefaultFont->DrawTextA(
    id3dxSpriteRenderer, str, -1,
    &r,
    DT_CALCRECT /* | dtOptions */, 0 ) ;
}

void Window::drawString( int fontId, char *str, D3DCOLOR color )
{
  RECT rect ;
  SetRect( &rect, 0, 0, getWidth(), getHeight() ) ;

  drawString( fontId, str, color, rect, DT_CENTER | DT_VCENTER ) ;
}

void Window::drawString( int fontId, char *str, D3DCOLOR color, float x, float y, float boxWidth, float boxHeight )
{
  RECT rect ;
  SetRect( &rect, x, y, x + boxWidth, y + boxHeight ) ;

  drawString( fontId, str, color, rect, DT_CENTER | DT_VCENTER ) ;
}

void Window::drawString( int fontId, char *str, D3DCOLOR color, float x, float y, float boxWidth, float boxHeight, DWORD formatOptions )
{
  RECT rect ;
  SetRect( &rect, x, y, x + boxWidth, y + boxHeight ) ;

  drawString( fontId, str, color, rect, formatOptions ) ;
}

void Window::drawString( int fontId, char *str, D3DCOLOR color, RECT &rect )
{
  drawString( fontId, str, color, rect, DT_CENTER | DT_VCENTER ) ;
}

void Window::drawString( int fontId, char *str, D3DCOLOR color, RECT &rect, DWORD formatOptions )
{
  // Retrieve the font
  ID3DXFont *font = id3dxDefaultFont ;

  if( fontId != DEFAULT_FONT )
  {
    FontMapIter fontEntry = fonts.find( fontId ) ;

    if( fontEntry == fonts.end() )
    {
      warning( "Font %d does not exist, using default font instead", fontId ) ;
    }
    else
    {
      font = fontEntry->second ;
    }
  }

  font->DrawTextA( NULL, str, -1, &rect, formatOptions, color ) ;

}

void Window::createFont( int id, char *fontName, float size, int boldness, bool italics )
{
  ID3DXFont *font = id3dxDefaultFont ;
  FontMapIter fontEntry = fonts.find( id ) ;

  if( fontEntry != fonts.end() )
  {
    warning( "Font %d already existed, destroying and replacing..", id ) ;
    SAFE_RELEASE( font ) ;
  }

  DX_CHECK( D3DXCreateFontA( gpu, size, 0, boldness, 1,
    italics, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY,
    DEFAULT_PITCH | FF_DONTCARE, fontName, &font ), "Create custom font" ) ;

  
  // Now add it to the map
  fonts.insert( make_pair( id, font ) ) ;

}

int Window::getWidth()
{
  return d3dpps.BackBufferWidth ;
}

int Window::getHeight()
{
  return d3dpps.BackBufferHeight ;
}

bool Window::fullScreenInMaxResolution()
{
  return setSize( GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ), true ) ;
}

bool Window::setSize( int width, int height, bool fullScreen )
{
  
  // Before trying to setSize(), remember the old (default) present parameters
  D3DPRESENT_PARAMETERS oldd3dpps = d3dpps ;

  // Now update with new ones.
  d3dpps.BackBufferWidth = width ;
  d3dpps.BackBufferHeight = height ;
  d3dpps.Windowed = !fullScreen ;

  // to change, we need to lose the old device,
  // and reset with the new parameters

  d3dLoseDevice() ;
  if( !d3dResetDevice( d3dpps ) )
  {
    // if the change fails, reset with old options
    warning( "Could not put device into resolution width=%d height=%d fullscreen=%d", width, height, fullScreen ) ;
    d3dpps = oldd3dpps ;
    
    warning( "Reverting to old settings" ) ;
    if( !d3dResetDevice( d3dpps ) )
    {
      // if THAT fails, then you have a serious problem
      error( "Serious problem resetting the device" ) ;
      
      // try and completely reset d3d to stock options
      d3dShutdown() ;
      if( !initD3D() )
      {
        bail( "Couldn't even re-initialize d3d" ) ;
      }
    }
  }


  // Ok, now that d3d buffer/device resize was successful,
  // resize the actual window now, if its in windowed mode

  RECT wndSize ;
  wndSize.left = 0 ;
  wndSize.right = width ;
  wndSize.top =  0 ;
  wndSize.bottom = height ;

  // tell the mouse about the resize, before you
  // adjust the rect
  mouse.setClipZone( wndSize ) ;


  // We have to AdjustWindowRectEx() so the client area
  // is exactly the right size
  AdjustWindowRectEx( &wndSize, WS_OVERLAPPEDWINDOW, NULL, 0 ) ;

  SetWindowPos( hwnd, HWND_TOP, 0, 0, wndSize.right - wndSize.left, wndSize.bottom - wndSize.top, 
    SWP_NOMOVE | SWP_NOZORDER ) ;



  return true ;
}

void Window::d3dLoseDevice()
{
  isDeviceLost = true ;

  // call the onLostDevice function of every id3dx object
  DX_CHECK( id3dxDefaultFont->OnLostDevice(), "font onlostdevice" ) ;
  DX_CHECK( id3dxSpriteRenderer->OnLostDevice(), "sprite renderer onlostdevice" ) ;
}

bool Window::d3dResetDevice( D3DPRESENT_PARAMETERS & pps )
{
  HRESULT hr ;

  hr = gpu->Reset( &d3dpps ) ;
  bool succeeded = DX_CHECK( hr, "reset gpu device" ) ; // this still might fail.
  // If it does fail, then the device is STILL lost then.

  // This is written out this way to make it clear
  // how the program logic goes.
  if( !succeeded )
    isDeviceLost = true ;
  else
    isDeviceLost = false ;

  // If the previous call to Reset the GPU _worked_, then
  // we should reset all the id3dx objects
  if( !isDeviceLost )
  {
    DX_CHECK( id3dxDefaultFont->OnResetDevice(), "font onlostdevice" ) ;
    DX_CHECK( id3dxSpriteRenderer->OnResetDevice(), "sprite renderer onlostdevice" ) ;

    info( "GPU reset complete" ) ;
  }
  else
  {
    error( "I could not reset the gpu" ) ;
  }

  return succeeded ;
}

void Window::d3dDeviceCheck()
{
  // check if the device is "lost"
  // if it is, we should not draw.

  HRESULT hr ;
  hr = gpu->TestCooperativeLevel() ;


  switch( hr )
  {
  case D3DERR_DRIVERINTERNALERROR:
    error( "Hmm, the driver experienced an internal error.  This is unusual." ) ;

    // Try and re-initialize d3d.  If it fails, quit.
    if( !initD3D() )
    {
      bail( "Experienced D3DERR_DRIVERINTERNALERROR" ) ;
    }
    break ;

  case D3DERR_DEVICELOST:
    if( !isDeviceLost )  // if the device wasn't already lost..
    {
      warning( "The device has just been lost" ) ;

      d3dLoseDevice() ;
    }
    break ;

  case D3DERR_DEVICENOTRESET:
    if( isDeviceLost )
    {
      // The device was lost, but now we have the chance
      // to reset it.
      info( "Resetting the gpu device.." ) ;
      
      d3dResetDevice( d3dpps ) ;
    }
    else
    {
      // This should not happen, but if it does, we want to know about it
      error( "Device wasn't lost, yet we were given D3DERR_DEVICENOTRESET" ) ;
    }
    break ;
  
  case D3D_OK:
  default:
    // Device is ok, so don't do anything here
    break ;
  }
  
}

void Window::d3dShutdown()
{
  SAFE_RELEASE( id3dxSpriteRenderer ) ;
  SAFE_RELEASE( id3dxDefaultFont ) ;
  SAFE_RELEASE( gpu ) ;
  SAFE_RELEASE( d3d ) ;
}

//!! fix this out
bool Window::d3dSupportsNonPowerOf2Textures()
{
  bool conditionallySupportsNonPow2Tex = caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL ;

  return conditionallySupportsNonPow2Tex ;
}




void Window::initDefaultSprite()
{
  IDirect3DTexture9 *tex ;
  int w = 16, h = 16 ;
  DX_CHECK( D3DXCreateTexture( gpu, w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex ), "Create default sprite" ) ;

  D3DLOCKED_RECT lockedRect ;

  DX_CHECK( tex->LockRect( 0, &lockedRect, NULL, 0 ), "Lock default sprite" ) ;
  DWORD *ptr = reinterpret_cast<DWORD*>(lockedRect.pBits) ;
  RECT topLeft = { 0, 0, w/2, h/2 } ; // ltrb
  RECT topRight = { w/2, 0, w, h/2 } ;
  RECT bottomLeft = { 0, h/2, w/2, h } ;
  RECT bottomRight = { w/2, h/2, w, h } ;

  D3DCOLOR red = D3DCOLOR_ARGB( 200, 255, 0, 0 ) ;
  D3DCOLOR green = D3DCOLOR_ARGB( 200, 0, 255, 0 ) ;
  D3DCOLOR blue = D3DCOLOR_ARGB( 200, 0, 0, 255 ) ;
  D3DCOLOR yellow = D3DCOLOR_ARGB( 200, 255, 255, 0 ) ;
  setRectangle( ptr, topLeft, w, h, &blue ) ;
  setRectangle( ptr, topRight, w, h, &yellow ) ;
  setRectangle( ptr, bottomLeft, w, h, &green ) ;
  setRectangle( ptr, bottomRight, w, h, &red ) ;
  
  DX_CHECK( tex->UnlockRect( 0 ), "Unlock default sprite" ) ;

  defaultSprite = new Sprite( w, h, tex ) ;
  //addSprite( 0, whitePixel ) ; // no need to save a ref in the sprites map.

}

void Window::initWhitePixel()
{
  IDirect3DTexture9 *tex ;
  DX_CHECK( D3DXCreateTexture( gpu, 1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex ), "Create 1x1 pixel patch" ) ;

  D3DLOCKED_RECT lockedRect ;

  DX_CHECK( tex->LockRect( 0, &lockedRect, NULL, 0 ), "Lock 1x1 pixel patch" ) ;
  ((__int32*)lockedRect.pBits)[0] = D3DCOLOR_ARGB( 255,255,255,255 ) ;
  DX_CHECK( tex->UnlockRect( 0 ), "Unlock 1x1 pixel patch" ) ;

  whitePixel = new Sprite( 1, 1, tex ) ;
  //addSprite( 0, whitePixel ) ; // no need to save a ref in the sprites map.

}

// You need this wrapper function to hide 'gpu'
GDIPlusTexture* Window::createGDISurface( int width, int height )
{
  GDIPlusTexture *gdiPlusTex = new GDIPlusTexture( gpu, width, height, true ) ;
  return gdiPlusTex ;
}

void Window::addSpriteFromGDIPlusTexture( int id, GDIPlusTexture* tex )
{
  Sprite *sprite = new Sprite(
    tex->getWidth(), tex->getHeight(), tex->getTexture() ) ;

  addSprite( id, sprite ) ;
}

void Window::step()
{
  // check the d3ddevice, in case its been lost
  d3dDeviceCheck() ;


  soundStep();


  // Advance sprite animations.
  // This is the only thing that
  // is PAUSED when the game is
  // PAUSED.  TODO:  Pause sounds here as well...
  if( !paused )
  {
    foreach( SpriteMapIter, iter, sprites )
    {
      iter->second->advance( timer.time_since_last_frame ) ;
    }
  }

  // Copy over "current" states to "previous" states
  //!! Should MOVE into Keyboard class
  memcpy( keyPreviousStates, keyCurrentStates, 256 ) ;

  // Grab all keystates, to know what the user is currently pushing down.
  if( !GetKeyboardState( keyCurrentStates ) )
  {
    printWindowsLastError( "GetKeyboardState()" ) ;
  }

  // step the mouse
  mouse.step();

  
  timer.lock( 60 ) ; // // ^^Leave as last line: YES, RECOMMENDED.  Use this line to LOCK FRAMERATE
  // at 60 fps max.  This will "waste" any idle time at the end of
  // processing a frame.
  
  //timer.update();  // NOT RECOMMENDED.  Use this line to SIMPLY UPDATE
  // the frame counter WITHOUT frame limiting.
  
  // This mode lets the game run AS FAST AS IT POSSIBLY CAN
  // on this machine, and you might see frame rates of
  // 300fps or so.  This means your game will vary in
  // speed though depending on how much "stuff" is on
  // the screen.  NOT RECOMMENDED becuase your game will
  // run at varying speeds on different machines.  also
  // you will see tearing, which doesn't look very good.
  
  // NOTE:  YOU MUST ALSO FIND AND UNCOMMENT THE LINE
  // that says FRAMERATE::UNBOUNDED



}

bool Window::isSlow()
{
  return timer.frames_per_second < 55 ;
}

float Window::getTimeElapsedSinceLastFrame()
{
  return timer.time_since_last_frame ;
}

void Window::setBackgroundColor( D3DCOLOR color )
{
  clearColor = color ;
}

void Window::drawMouseCursor(int spriteId, bool showCursorCoordinates)
{
  drawSprite( spriteId, mouse.getX(), mouse.getY(), SpriteCentering::TopLeft ) ;

  if( showCursorCoordinates )
  {
    char buf[ 300 ] ;
    sprintf( buf, "mouse\n(%d, %d)", mouse.getX(), mouse.getY() ) ;

    RECT r ;
    getBoxDimensions( buf, r ) ;
    drawBox( D3DCOLOR_ARGB( 120, 255, 255, 255 ), mouse.getX(), mouse.getY(), r.right-r.left, r.bottom-r.top ) ;
    drawString( DEFAULT_FONT, buf, D3DCOLOR_ARGB( 255, 0, 0, 120 ), mouse.getX(), mouse.getY(), r.right-r.left, r.bottom-r.top, DT_LEFT | DT_TOP ) ;
  }
}

// Show fps
void Window::drawFrameCounter()
{
  static char buf[ 60 ];
  sprintf( buf, "FPS:  %.1f", timer.frames_per_second ) ;

  /*
  // There's no point in computing this.  its a jittery box.
  RECT r ;
  getBoxDimensions( buf, r ) ;
  
  r.left = getWidth() - r.right - 10  ;
  r.right = getWidth() - 10 ;
  r.top += 10 ;
  r.bottom += 10 ;
  */
  
  int left = getWidth() - 10 - 100 ;
  drawBox( D3DCOLOR_ARGB( 235, 0, 0, 128 ), left, 10, 100, 30 ) ;
  drawString( DEFAULT_FONT, buf, Color::White, left, 10, 100, 30 ) ;
}


// Some may argue that you could expose the "paused"
// variable as a public member.. but using member functions
// like this allows you to actually perform "clean-up" type
// stuff on pause or unpause.  For example, here we're calling
// FMOD's pause function.  We get the opportunity to do it
// once, immediately as the pause() function is called.
// See?  OOP isn't all that bad!
void Window::pause()
{
  // If the game wasn't already paused..
  if( !paused )
  {
    // ..then pause everything
    soundPause() ;

    paused = true ;  // this member is checked
    // in Window::step()
  }
}
void Window::unpause()
{
  if( paused )
  {
    soundUnpause() ;

    paused = false ;
  }
}
bool Window::isPaused()
{
  return paused ;
}


int Window::getMouseX()
{
  return mouse.getX() ;
}
int Window::getMouseY()
{
  return mouse.getY() ;
}

// Returns true if key is DOWN this frame
// and was UP previous frame.
bool Window::keyJustPressed( int vkCode )
{
  return KEY_IS_DOWN( keyCurrentStates, vkCode ) &&  // Down this frame, &&
         KEY_IS_UP( keyPreviousStates, vkCode ) ;     // AND up previous frame
  
  // bitwise AND logical AND's used here.

  // Two important bits of information from
  // GetKeyboardState() documentation on MSDN:
  // "If the high-order bit is 1,
  //  the key is down;
  //  otherwise, it is up."

  // "The low-order bit is meaningless for non-toggle keys."

}

// Tells you if a key is BEING HELD DOWN
bool Window::keyIsPressed( int vkCode )
{
  return KEY_IS_DOWN( keyCurrentStates, vkCode ) ;
}

// Returns true if a key was JUST let go of.
// A complimentary function to justPressed()
bool Window::keyJustReleased( int vkCode )
{
  return KEY_IS_DOWN( keyPreviousStates, vkCode ) &&  // Key __WAS__ down AND
         KEY_IS_UP( keyCurrentStates, vkCode ) ;      // KEY IS UP NOW
}

void Window::mouseUpdateInput( RAWINPUT * raw ) 
{
  mouse.updateInput( raw ) ;
}
bool Window::mouseJustPressed( Mouse::Button button )
{
  return mouse.justPressed( button ) ;
}
bool Window::mouseIsPressed( Mouse::Button button )
{
  return mouse.isPressed( button ) ;
}
bool Window::mouseJustReleased( Mouse::Button button )
{
  return mouse.justReleased( button ) ;
}




/////////////// PRIVATE ///////////////////
void Window::startupRawInput()
{
  // After the window has been created, 
  // register raw input devices
  RAWINPUTDEVICE Rid[2] ;
        
  Rid[0].usUsagePage = 0x01 ;
  Rid[0].usUsage = 0x02 ;
  
  ////Rid[0].dwFlags = 0 ; // (use this if you DO NOT WANT to capture mouse)
  Rid[0].dwFlags = RIDEV_CAPTUREMOUSE | RIDEV_NOLEGACY ;  // (use this to CAPTURE MOUSE)

  // RIDEV_CAPTUREMOUSE makes it so we
  // SEIZE UP THE MOUSE from the rest of the system.
  // This makes it so the user cannot accidently "click away"
  // from your app, which is good.

  // If you want the mouse to be "captured",
  // just set Rid[0].dwFlags=0;
  // You should also comment out the line
  // that says ShowCursor( FALSE ) ; if you
  // want the normal windows white mouse cursor
  // to show

  // To use this mode we must also specify RIDEV_NOLEGACY mode.
  // RIDEV_NOLEGACY makes it so we WILL NOT
  // get WM_LBUTTONDOWN ("old-school" aka 'legacy') messages.
  // Instead we will just get WM_INPUT messages.
  Rid[0].hwndTarget = hwnd ;

  // We don't really need raw input for the keyboard
  // but it is nice to have it hooked up because
  // 
  Rid[1].usUsagePage = 0x01 ;
  Rid[1].usUsage = 0x06 ;
  Rid[1].dwFlags = 0 ; // RIDEV_NOHOTKEYS ;  // use the RIDEV_NOHOTKEYS
  // option to turn off WINKEY
  
  // Also, for the keyboard, DO NOT specify RIDEV_NOLEGACY.
  // If you do, you will no longer be able to "hear"
  // WM_CHAR messages.  WM_CHAR messages are the best and
  // easiest way to get correct typing keystrokes
  // with UpPerCasEd aNd LowErCaseD letters as the user typed them.
  Rid[1].hwndTarget = hwnd ;

  if( !RegisterRawInputDevices( Rid, 2, sizeof(Rid[0]) ) )
  {
    //registration failed. Check your Rid structs above.
    printWindowsLastError( "RegisterRawInputDevices" ) ;
    bail( "Could not register raw input devices. Check your Rid structs, please." ) ;
  }


  // clear keystates
  memset( keyCurrentStates, 0, 256 ) ;
  memset( keyPreviousStates, 0, 256 ) ;
}