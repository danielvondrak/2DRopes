/*
 * NOTE: THINGS TO LOOK UP
 *  -> UNICODE COMPILATION
 *	-> MEMORY ALLOCATION METHODOLOGY
 *
 * TODO:
 * 	-> Saved game locations
 * 	-> Getting a handle to our own executable file
 * 	-> Asset loading path
 * 	-> Threading
 * 	-> Raw Input (support for multiple keyboards)
 * 	-> ClipCursor (for multimonitor support)
 * 	-> Fullscreen support
 * 	-> WM_SETCURSOR (cursor control visibility)
 * 	-> Blit speed improvements
 * 	-> Hardware acceleration (OpenGL or Direct3D)
 * 	-> Disconnect/Reconnect GamePad
 *
 * TODO: BUGS
 * -> 144hz refresh rate causes audio scratching.
 * */
#include "DV_Platform.h"

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "Win32App.h"

//globals
global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;
global_variable bool32 DEBUGGlobalShowCursor;
global_variable WINDOWPLACEMENT GlobalWindowPosition = {sizeof(GlobalWindowPosition)};


//UTILITY FUNCTIONS
internal void CatStrings(size_t SourceACount, char *SourceA, size_t SourceBCount, char *SourceB, size_t DestCount, char *Dest)
{
	for(int Index = 0; Index < SourceACount; ++Index)
	{
		*Dest++ = *SourceA++;
	}

	for(int Index = 0; Index < SourceBCount; ++Index)
	{
		*Dest++ = *SourceB++;
	}

	*Dest++ = 0;
}

internal int StringLength(char *String)
{
	int Count = 0;
	while(*String++)
	{
		++Count;
	}
	return Count;
}

internal void Win32GetEXEFilename(win32_state *Win32State)
{
	DWORD SizeOfFilename = GetModuleFileNameA(0, Win32State->EXEFilename, sizeof(Win32State->EXEFilename));
	Win32State->OnePastLastEXEFilenameSlash = Win32State->EXEFilename;
	for(char *Scan = Win32State->EXEFilename; *Scan; ++Scan)
	{
		if(*Scan == '\\')
		{
			Win32State->OnePastLastEXEFilenameSlash = Scan + 1;
		}
	}
}


internal void Win32BuildEXEPathFilename(win32_state *Win32State, char *Filename, int DestCount, char *Dest)
{
	CatStrings(Win32State->OnePastLastEXEFilenameSlash - Win32State->EXEFilename, Win32State->EXEFilename,
			StringLength(Filename), Filename,
			DestCount, Dest);
}
// Controller stuff
// macro to create a function with a variable name
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) // create stub function
{
  return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// Macro for XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) // create stub function
{
  return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_


// AUDIO INITIALIZATION
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
  if (Memory)
  {
    VirtualFree(Memory, 0, MEM_RELEASE);
  }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
  debug_read_file_result Result = {};

  HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if (FileHandle != INVALID_HANDLE_VALUE)
  {
    LARGE_INTEGER FileSize;
    if (GetFileSizeEx(FileHandle, &FileSize))
    {
      uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
      Result.Contents = VirtualAlloc(0, FileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      if (Result.Contents)
      {
        DWORD BytesRead;
        if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead))
        {
          // File Read succesfully
          Result.ContentsSize = FileSize32;

        } else
        {
          // File Read Failed
          DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
          Result.Contents = 0;
        }
      } else
      {
        // Logging
      }
    }
    CloseHandle(FileHandle);
  }
  return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
  bool32 Result = 0;

  HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if (FileHandle != INVALID_HANDLE_VALUE)
  {
    DWORD BytesWritten;
    if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
    {
      // File Read succesfully
      Result = (BytesWritten == MemorySize);

    } else
    {
      // Logging
    }
    CloseHandle(FileHandle);
  } else
  {
    // Logging
  }
  return Result;
}

inline FILETIME Win32GetLastWriteTime(char *filename)
{
  FILETIME LastWriteTime = {};
  WIN32_FILE_ATTRIBUTE_DATA Data;
  if(GetFileAttributesEx(filename, GetFileExInfoStandard, &Data))
  {
	  LastWriteTime = Data.ftLastWriteTime;
  }
  return LastWriteTime;
}

internal win32_game_code Win32LoadGameCode(char *SourceDLLName, char *TempDLLName, char *LockFileName) {

  struct win32_game_code Result = {};

  WIN32_FILE_ATTRIBUTE_DATA Ignored;
  if (!GetFileAttributesEx(LockFileName, GetFileExInfoStandard, &Ignored))
  {
    Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);
    CopyFile(SourceDLLName, TempDLLName, 0);

    Result.GameCodeDLL = LoadLibraryA(TempDLLName);
    if (Result.GameCodeDLL)
    {
      Result.GetSoundSamples = (game_get_sound_samples *)GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");
      Result.UpdateAndRender = (game_update_and_render *)GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");

      Result.IsValid = (Result.GetSoundSamples && Result.UpdateAndRender);
    }
  }

  if (!Result.IsValid)
  {
    Result.UpdateAndRender = 0;
    Result.GetSoundSamples = 0;
  }
  return Result;
}

internal void Win32UnloadGameCode(win32_game_code *GameCode) {

	if(GameCode->GameCodeDLL)
	{
		FreeLibrary(GameCode->GameCodeDLL);
		GameCode->GameCodeDLL = 0;
	}
	GameCode->IsValid = 0;
	GameCode->GetSoundSamples = 0;
	GameCode->UpdateAndRender = 0;
}

internal void Win32LoadXInput(void) {
  // TODO: Test this on Windows 8
  HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
  if (!XInputLibrary)
  {
    // TODO: Diagnostic
    XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
  }

  if (!XInputLibrary)
  {
    // TODO: Diagnostic
    XInputLibrary = LoadLibraryA("xinput1_3.dll");
  }

  if (XInputLibrary)
  {
    XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
    if (!XInputGetState)
    {
      XInputGetState = XInputGetStateStub;
    }

    XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    if (!XInputSetState)
    {
      XInputSetState = XInputSetStateStub;
    }

    // TODO: Diagnostic

  } else
  {
    // TODO: Diagnostic
  }
}

internal void Win32InitDSound(HWND Window, int32 BufferSize, int32 SamplesPerSecond) {
  HMODULE DSoundLibrary = LoadLibrary("dsound.dll");
  if (DSoundLibrary)
  {
    direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
    LPDIRECTSOUND DirectSound;

    if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(NULL, &DirectSound, NULL)))
    {
      WAVEFORMATEX WaveFormat = {};
      WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
      WaveFormat.nChannels = 2;
      WaveFormat.nSamplesPerSec = SamplesPerSecond;
      WaveFormat.wBitsPerSample = 16;
      WaveFormat.cbSize = 0;
      WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
      WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;

      if (SUCCEEDED(DirectSound->lpVtbl->SetCooperativeLevel(DirectSound, Window, DSSCL_PRIORITY)))
      {
        DSBUFFERDESC BufferDesc = {}; // sets values to zero somehow...
        BufferDesc.dwSize = sizeof(BufferDesc);
        BufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
        LPDIRECTSOUNDBUFFER PrimaryBuffer;
        if (SUCCEEDED(DirectSound->lpVtbl->CreateSoundBuffer(DirectSound, &BufferDesc, &PrimaryBuffer, NULL)))
        {
          if (SUCCEEDED(PrimaryBuffer->lpVtbl->SetFormat(PrimaryBuffer, &WaveFormat)))
          {
            // SUCCESSFULLY Created primary buffer
            printf("succesful primary buffer!\n");
          } else
          {
            // TODO: Error handle SetFormat
          }
        } else
        {
          // TODO: Error handle Create Primary Sound Buffer
        }
      } else
      {
        // TODO: Error handle Set Cooperative Level
      }

      DSBUFFERDESC BufferDesc = {}; // sets values to zero somehow...
      BufferDesc.dwSize = sizeof(BufferDesc);
      BufferDesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
      BufferDesc.dwBufferBytes = BufferSize;
      BufferDesc.lpwfxFormat = &WaveFormat;
      HRESULT error = DirectSound->lpVtbl->CreateSoundBuffer(DirectSound, &BufferDesc, &GlobalSecondaryBuffer, NULL);
      if (SUCCEEDED(error))
      {
        printf("Successful Secondary buffer\n");
      } else
      {
        printf("error message: %ld\n", error);
      }
    } else
    {
      // TODO: Error Handle DirectSound Create
    }
  }
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window) {
  win32_window_dimension Result;

  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  Result.Width = ClientRect.right - ClientRect.left;
  Result.Height = ClientRect.bottom - ClientRect.top;

  return (Result);
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    // TODO(casey): Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.

    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    int BytesPerPixel = 4;
    Buffer->BytesPerPixel = BytesPerPixel;

    // NOTE(casey): When the biHeight field is negative, this is the clue to
    // Windows to treat this bitmap as top-down, not bottom-up, meaning that
    // the first three bytes of the image are the color for the top left pixel
    // in the bitmap, not the bottom left!
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    // NOTE(casey): Thank you to Chris Hecker of Spy Party fame
    // for clarifying the deal with StretchDIBits and BitBlt!
    // No more DC for us.
    int BitmapMemorySize = (Buffer->Width*Buffer->Height)*BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width*BytesPerPixel;

    // TODO(casey): Probably clear this to black
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
                           HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    // TODO: This does not work on non-1080p displays.
    
    if((WindowWidth >= Buffer->Width*2) &&
       (WindowHeight >= Buffer->Height*2))
    {
        StretchDIBits(DeviceContext,
                      0, 0, 2*Buffer->Width, 2*Buffer->Height,
                      0, 0, Buffer->Width, Buffer->Height,
                      Buffer->Memory,
                      &Buffer->Info,
                      DIB_RGB_COLORS, SRCCOPY);
    }
    else
    {
        int OffsetX = 10;
        int OffsetY = 10;

        PatBlt(DeviceContext, 0, 0, WindowWidth, OffsetY, BLACKNESS);
        PatBlt(DeviceContext, 0, OffsetY + Buffer->Height, WindowWidth, WindowHeight, BLACKNESS);
        PatBlt(DeviceContext, 0, 0, OffsetX, WindowHeight, BLACKNESS);
        PatBlt(DeviceContext, OffsetX + Buffer->Width, 0, WindowWidth, WindowHeight, BLACKNESS);
    
        // NOTE(casey): For prototyping purposes, we're going to always blit
        // 1-to-1 pixels to make sure we don't introduce artifacts with
        // stretching while we are learning to code the renderer!
        StretchDIBits(DeviceContext,
                      OffsetX, OffsetY, Buffer->Width, Buffer->Height,
                      0, 0, Buffer->Width, Buffer->Height,
                      Buffer->Memory,
                      &Buffer->Info,
                      DIB_RGB_COLORS, SRCCOPY);
    }
}

LRESULT CALLBACK MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  LRESULT result = 0;
  switch (message)
  {
    case WM_SIZE:
      {
      }
      break;
		case WM_SETCURSOR:
			{
				if(DEBUGGlobalShowCursor)
				{
        	result = DefWindowProc(window, message, wParam, lParam);
				}
				else
				{
					SetCursor(0);
				}
			}
			break;
    case WM_PAINT:
      {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(window, &Paint);
        win32_window_dimension Dimension = Win32GetWindowDimension(window);
        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
        EndPaint(window, &Paint);
      }
      break;
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
      {
        Assert(!"Keyboard input was processed in a non-dispatch message");
      }
      break;
    case WM_DESTROY:
      {
        GlobalRunning = 0;
      }
      break;
    case WM_CLOSE:
      {
        GlobalRunning = 0;
      }
      break;
    case WM_ACTIVATEAPP:
      {
#if 0
		  if(wParam == TRUE)
		  {
			SetLayeredWindowAttributes(window, RGB(0,0,0), 255, LWA_ALPHA);
		  } else
		  {
			SetLayeredWindowAttributes(window, RGB(0,0,0), 64, LWA_ALPHA);
		  }
#endif
      }
      break;
    default:
      {
        result = DefWindowProc(window, message, wParam, lParam);
      }
      break;
  }
  return result;
}

internal void Win32ClearSoundBuffer(win32_sound_output *SoundOutput) {
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;
  if (SUCCEEDED(GlobalSecondaryBuffer->lpVtbl->Lock(GlobalSecondaryBuffer, 0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
  {
    uint8 *DestSample = (uint8 *)Region1;
    for (DWORD SampleIndex = 0; SampleIndex < Region1Size; ++SampleIndex)
    {
      *DestSample++ = 0;
    }
    DestSample = (uint8 *)Region2;
    for (DWORD SampleIndex = 0; SampleIndex < Region2Size; ++SampleIndex)
    {
      *DestSample++ = 0;
    }
    GlobalSecondaryBuffer->lpVtbl->Unlock(GlobalSecondaryBuffer, Region1, Region1Size, Region2, Region2Size);
  }
}

// This method takes the sound buffer created by the Game layer and copies it to the platform(win32) buffer.
internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer *GameSoundBuffer) {

  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;
  if (SUCCEEDED(GlobalSecondaryBuffer->lpVtbl->Lock(GlobalSecondaryBuffer, ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
  {
    DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
    int16 *DestSample = (int16 *)Region1;
    int16 *SourceSample = GameSoundBuffer->Samples;
    for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
    {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      ++SoundOutput->RunningSampleIndex;
    }
    DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
    DestSample = (int16 *)Region2;
    for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
    {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      ++SoundOutput->RunningSampleIndex;
    }
    GlobalSecondaryBuffer->lpVtbl->Unlock(GlobalSecondaryBuffer, Region1, Region1Size, Region2, Region2Size);
  } else
  {
    // TODO: Error Handle Lock
  }
}

internal void Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown) {
  if (NewState->EndedDown != IsDown)
  {
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
  }
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state *OldState, DWORD ButtonBit, game_button_state *NewState) {
  NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
  NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold) {
  real32 Result = 0;

  if (Value < -DeadZoneThreshold)
  {
    Result = (real32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
  } else if (Value > DeadZoneThreshold)
  { Result = (real32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold)); }

  return (Result);
}

internal void Win32GetInputFileLocation(win32_state *Win32State, bool32 InputStream,  int SlotIndex, int DestCount, char *Dest) {
  char Temp[64];
  wsprintf(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
  Win32BuildEXEPathFilename(Win32State, Temp, DestCount, Dest);
}

internal win32_replay_buffer* Win32GetReplayBuffer(win32_state *Win32State, int unsigned ReplayIndex)
{
  Assert(ReplayIndex < ArrayCount(Win32State->ReplayBuffers)); 
  win32_replay_buffer *ReplayBuffer = &Win32State->ReplayBuffers[ReplayIndex];
  return ReplayBuffer;
}

internal void Win32BeginRecordingInput(win32_state *Win32State, int InputRecordingIndex) {

	win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(Win32State, InputRecordingIndex);
  if (ReplayBuffer->MemoryBlock)
  {
    Win32State->InputRecordingIndex = InputRecordingIndex;

	char Filename[WIN32_STATE_FILE_NAME_COUNT];
	Win32GetInputFileLocation(Win32State, 1, InputRecordingIndex, sizeof(Filename), Filename);
    Win32State->RecordingHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

    // LARGE_INTEGER FilePosition;
    // FilePosition.QuadPart = Win32State->TotalSize;
    // SetFilePointerEx(Win32State->RecordingHandle, FilePosition, 0, FILE_BEGIN);
    CopyMemory(Win32State->ReplayBuffers[InputRecordingIndex].MemoryBlock, Win32State->GameMemoryBlock, Win32State->TotalSize);
  }
}

internal void Win32EndRecordingInput(win32_state *Win32State)
{
	CloseHandle(Win32State->RecordingHandle);
	Win32State->InputRecordingIndex = 0;
}

internal void Win32BeginInputPlayback(win32_state *Win32State, int InputPlayingIndex) {

  win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(Win32State, InputPlayingIndex);
  if (ReplayBuffer->MemoryBlock)
  {
    Win32State->InputPlayingIndex = InputPlayingIndex;
	char Filename[WIN32_STATE_FILE_NAME_COUNT];
	Win32GetInputFileLocation(Win32State, 1, InputPlayingIndex, sizeof(Filename), Filename);
    Win32State->PlaybackHandle = CreateFileA(Filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

    // LARGE_INTEGER FilePosition;
    // FilePosition.QuadPart = Win32State->TotalSize;
    // SetFilePointerEx(Win32State->PlaybackHandle, FilePosition, 0, FILE_BEGIN);
	CopyMemory(Win32State->GameMemoryBlock,ReplayBuffer->MemoryBlock, Win32State->TotalSize);
  }
}

internal void Win32EndInputPlayback(win32_state *Win32State)
{
	CloseHandle(Win32State->PlaybackHandle);
	Win32State->InputPlayingIndex = 0;
}

internal void Win32RecordInput(win32_state *Win32State, game_input *NewInput)
{
	DWORD BytesWritten;
	WriteFile(Win32State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);

}
internal void Win32PlaybackInput(win32_state *Win32State, game_input *NewInput) {
  DWORD BytesRead;
  if (ReadFile(Win32State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
  {
    if (BytesRead == 0)
    {
      int PlayingIndex = Win32State->InputPlayingIndex;
      Win32EndInputPlayback(Win32State);
      Win32BeginInputPlayback(Win32State, PlayingIndex);
      ReadFile(Win32State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
    }
  }
}

internal void
ToggleFullscreen(HWND Window)
{
    // NOTE: This follows Raymond Chen's prescription
    // for fullscreen toggling, see:
    // http://blogs.msdn.com/b/oldnewthing/archive/2010/04/12/9994016.aspx
    
    DWORD Style = GetWindowLong(Window, GWL_STYLE);
    if(Style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO MonitorInfo = {sizeof(MonitorInfo)};
        if(GetWindowPlacement(Window, &GlobalWindowPosition) &&
           GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
        {
            SetWindowLong(Window, GWL_STYLE, Style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(Window, HWND_TOP,
                         MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalWindowPosition);
        SetWindowPos(Window, 0, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

internal void Win32ProcessPendingMessages(win32_state *Win32State, game_controller_input *KeyboardController) {
  MSG Message;
  while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
  {
    switch (Message.message)
    {
      case WM_QUIT:
        {
          GlobalRunning = 0;
        }
        break;

      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_KEYUP:
        {
          uint32 VKCode = (uint32)Message.wParam;
          bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
          bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);
          if (WasDown != IsDown)
          {
            if (VKCode == 'W')
            {
              Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
            } else if (VKCode == 'A')
            {
              Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
            } else if (VKCode == 'S')
            {
              Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
            } else if (VKCode == 'D')
            {
              Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
            } else if (VKCode == 'Q')
            {
              Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
            } else if (VKCode == 'E')
            {
              Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);

            } else if (VKCode == VK_UP)
            {
              Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
            } else if (VKCode == VK_LEFT)
            {
              Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
            } else if (VKCode == VK_DOWN)
            {
              Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
            } else if (VKCode == VK_RIGHT)
            {
              Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
            } else if (VKCode == VK_ESCAPE)
            {
              Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
            } else if (VKCode == VK_SPACE)
            {
              Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
            }
#if INTERNAL
            else if (VKCode == 'P')
            {
              if (IsDown)
              {
                GlobalPause = !GlobalPause;
              }
            } else if (VKCode == 'L')
            {
              if (IsDown)
              {
                if (Win32State->InputPlayingIndex == 0)
                {
                  if (Win32State->InputRecordingIndex == 0)
                  {
                    Win32BeginRecordingInput(Win32State, 1);
                  } else
                  {
                    Win32EndRecordingInput(Win32State);
                    Win32BeginInputPlayback(Win32State, 1);
                  }
                } else
                {
                  Win32EndInputPlayback(Win32State);
                }
              }
            }
#endif

            if (IsDown)
            {
              bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
              if ((VKCode == VK_F4) && AltKeyWasDown)
              {
                GlobalRunning = 0;
              }
              if ((VKCode == VK_RETURN) && AltKeyWasDown)
              {
                if (Message.hwnd)
                {
                  ToggleFullscreen(Message.hwnd);
                }
              }
            }
          }
        }
        break;

      default:
        {
          TranslateMessage(&Message);
          DispatchMessageA(&Message);
        }
        break;
    }
  }
}

  inline LARGE_INTEGER Win32GetWallClock() {
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result;
  }

  inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {

    real32 Result = ((real32)(End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCountFrequency); // Seconds it took to do All of the Game Work this frame.
    return Result;
  }

#if 0
  internal void Win32DebugDrawVertical(win32_offscreen_buffer * Backbuffer, int X, int Top, int Bottom, uint32 Color) {
    if(Top <= 0)
    {
        Top = 0;
    }

    if(Bottom > Backbuffer->Height)
    {
        Bottom = Backbuffer->Height;
    }
    
    if((X >= 0) && (X < Backbuffer->Width))
    {
        uint8 *Pixel = ((uint8 *)Backbuffer->Memory +
                        X*Backbuffer->BytesPerPixel +
                        Top*Backbuffer->Pitch);
        for(int Y = Top;
            Y < Bottom;
            ++Y)
        {
            *(uint32 *)Pixel = Color;
            Pixel += Backbuffer->Pitch;
        }
    }
}

inline void
Win32DrawSoundBufferMarker(win32_offscreen_buffer *Backbuffer,
                           win32_sound_output *SoundOutput,
                           real32 C, int PadX, int Top, int Bottom,
                           DWORD Value, uint32 Color)
{
    real32 XReal32 = (C * (real32)Value);
    int X = PadX + (int)XReal32;
    Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, Color);
}

internal void
Win32DebugSyncDisplay(win32_offscreen_buffer *Backbuffer,
                      int MarkerCount, win32_debug_time_marker *Markers,
                      int CurrentMarkerIndex,
                      win32_sound_output *SoundOutput, real32 TargetSecondsPerFrame)
{
    int PadX = 16;
    int PadY = 16;

    int LineHeight = 64;
    
    real32 C = (real32)(Backbuffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryBufferSize;
    for(int MarkerIndex = 0;
        MarkerIndex < MarkerCount;
        ++MarkerIndex)
    {
        win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
        Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputByteCount < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);

        DWORD PlayColor = 0xFFFFFFFF;
        DWORD WriteColor = 0xFFFF0000;
        DWORD ExpectedFlipColor = 0xFFFFFF00;
        DWORD PlayWindowColor = 0xFFFF00FF;

        int Top = PadY;
        int Bottom = PadY + LineHeight;
        if(MarkerIndex == CurrentMarkerIndex)
        {
            Top += LineHeight+PadY;
            Bottom += LineHeight+PadY;

            int FirstTop = Top;
            
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayColor);
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);

            Top += LineHeight+PadY;
            Bottom += LineHeight+PadY;

            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);

            Top += LineHeight+PadY;
            Bottom += LineHeight+PadY;

            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
        }        
        
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480*SoundOutput->BytesPerSample, PlayWindowColor);
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
    }
}
#endif


// MAIN ENTRYPOINT
int WINAPI WinMain(HINSTANCE hInstance,     // handle to an instance. Unique ID for the exe.
                   HINSTANCE hPrevInstance, // always 0
                   PSTR pCmdLine,           // command-line arguments as a Unicode string
                   int nCmdShow)            // flag that indicates if the main application window is
                                            // minimized, maximized, or normal
{
  	win32_state Win32State = {};
	Win32GetEXEFilename(&Win32State);
	char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildEXEPathFilename(&Win32State, "DV.dll", sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);
	char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildEXEPathFilename(&Win32State, "DV_Temp.dll", sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);
	char GameCodeLockFullPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildEXEPathFilename(&Win32State, "lock.tmp", sizeof(GameCodeLockFullPath), GameCodeLockFullPath);

  // Performance Monitoring
  LARGE_INTEGER PerfCountFrequencyResult;
  QueryPerformanceFrequency(&PerfCountFrequencyResult);
  GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

  // Set the windwos scheduler granularity to 1 ms so that our sleep can be more granular
    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

  // XINPUT
  Win32LoadXInput();

  // Set Window DIB Section Size
  // this is the size of the window that can be drawn to.
  Win32ResizeDIBSection(&GlobalBackBuffer, 960, 540);

#if INTERNAL
	DEBUGGlobalShowCursor = 1;
#endif
  // Win32 Window Class info
  WNDCLASS windowClass = {};
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = MainWindowCallback;
  windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor(0, IDC_ARROW);
  windowClass.lpszClassName = TEXT("GAME");


  if (!RegisterClass(&windowClass))
  {
    // TODO Error Handle
    return 0;
  }

  //TODO: How do we reliably query the refresh rate on windows?

  HWND WindowHandle = CreateWindowEx(
		  							0, //WS_EX_TOPMOST | WS_EX_LAYERED,
		  							TEXT("GAME"), 
		  							windowClass.lpszClassName, 
									WS_OVERLAPPEDWINDOW, 
									CW_USEDEFAULT, 
									CW_USEDEFAULT, 
									CW_USEDEFAULT, 
									CW_USEDEFAULT, 
									0, 0, hInstance, 0);

  if (!WindowHandle)
  {
    // TODO: Error Handle for CreateWindowEx
    return 0;
  }
// TODO: 144hz Refresh Rate currently causes audio bugs.
  int MonitorRefreshHz = 60;
  HDC RefreshDC = GetDC(WindowHandle);
  int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
  ReleaseDC(WindowHandle, RefreshDC);
  if(Win32RefreshRate > 1)
  {
#if 0
	  MonitorRefreshHz = Win32RefreshRate;
#else
	  MonitorRefreshHz = 60;
#endif
  }

	real32 GameUpdateHz  = (MonitorRefreshHz / 1.0f);
  real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

  ShowWindow(WindowHandle, nCmdShow);

  // Loop Variables
  Win32State.InputRecordingIndex = 0;
  Win32State.InputPlayingIndex = 0;
  GlobalRunning = 1;
  int XOffset = 0;
  int YOffset = 0;

  // Win32InitXAudio2();
  // Sound Init
  win32_sound_output SoundOutput = {};
  SoundOutput.SamplesPerSecond = 48000;
  SoundOutput.BytesPerSample = sizeof(int16)*2;
  SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
  SoundOutput.SafetyBytes = (int)((real32)(SoundOutput.SamplesPerSecond * (real32)SoundOutput.BytesPerSample) / (real32)GameUpdateHz / 3.0f);
  Win32InitDSound(WindowHandle, SoundOutput.SecondaryBufferSize, SoundOutput.SamplesPerSecond);
  Win32ClearSoundBuffer(&SoundOutput);
  GlobalSecondaryBuffer->lpVtbl->Play(GlobalSecondaryBuffer, 0, 0, DSBPLAY_LOOPING);

#if 0
            // NOTE: This tests the PlayCursor/WriteCursor update frequency
            // On the Handmade Hero machine, it was 480 samples.
            while(GlobalRunning)
            {
                DWORD PlayCursor;
                DWORD WriteCursor;
                GlobalSecondarySoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);

                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer),
                            "PC:%u WC:%u\n", PlayCursor, WriteCursor);
                OutputDebugStringA(TextBuffer);
            }
#endif

  // MEMORY ALLOCATION
  //  Virtual Alloc for sound buffer
  int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

  // Game Memory Allocation
#if INTERNAL
  LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2); // Game Memory is a fixed location for debugging.
#else
  LPVOID BaseAddress = 0;
#endif

  // TODO: Handle various memory footprints (ie the user doesnt have enough RAM)
  game_memory GameMemory = {};
  GameMemory.PermanentStorageSize = Megabytes(64);
  GameMemory.TransientStorageSize = Gigabytes(1);
  GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
  GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
  GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

  //TODO: Look into using MEM_LARGE_PAGES and call Adjust token priveliges
  Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
  Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
  GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

  for (int ReplayIndex = 0; ReplayIndex < ArrayCount(Win32State.ReplayBuffers); ++ReplayIndex)
  {
    win32_replay_buffer *ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];
	
    Win32GetInputFileLocation(&Win32State, 0, ReplayIndex, sizeof(ReplayBuffer->Filename), ReplayBuffer->Filename);

    ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->Filename, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

	LARGE_INTEGER MaxSize;
	MaxSize.QuadPart = Win32State.TotalSize; //using LARGE_INTEGER union to split the high and low part of 64bit value
    ReplayBuffer->MemoryMap = CreateFileMapping(ReplayBuffer->FileHandle, 0, PAGE_READWRITE, MaxSize.HighPart, MaxSize.LowPart, 0);
    ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, Win32State.TotalSize);

    if (ReplayBuffer->MemoryBlock)
    {
    } else
    {
      // TODO: Logging
    }
  }

  if (!Samples || !GameMemory.PermanentStorage || !GameMemory.TransientStorage)
  {
    // TODO: Logging
    return 0;
  }

  game_input Input[2] = {};
  game_input *NewInput = &Input[0];
  game_input *OldInput = &Input[1];

  //used for debuggin audio
  int DebugTimeMarkerIndex = 0;
  win32_debug_time_marker DebugTimeMarkers[30] = {0};
  bool32 SoundIsValid = 0;

  DWORD AudioLatencyBytes = 0;
  real32 AudioLatencySeconds = 0;
  // High res (<uS) Performance Counter for time interval measurements
  LARGE_INTEGER LastCounter = Win32GetWallClock();
  LARGE_INTEGER FlipWallClock = Win32GetWallClock();
  // Returns the Processor Time Stamp. Records number of clock cycles since last reset.
  uint64 LastCycleCount = __rdtsc();

  win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath, GameCodeLockFullPath);

  /* ======================= GAME LOOP ======================= */

  while (GlobalRunning)
  {
	  FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
    if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
    {
      Win32UnloadGameCode(&Game);
      Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath, GameCodeLockFullPath);
    }

    // INPUT
    // Keyboard input
	NewInput->dtForFrame = TargetSecondsPerFrame;
    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
    *NewKeyboardController = (struct game_controller_input){0};
    NewKeyboardController->IsConnected = 1;
    for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
    {
      NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
    }

    Win32ProcessPendingMessages(&Win32State, NewKeyboardController);

    if (!GlobalPause)
    {
		POINT MouseP;
		GetCursorPos(&MouseP);
		ScreenToClient(WindowHandle, &MouseP);
		NewInput->MouseX = MouseP.x;
		NewInput->MouseY = MouseP.y;
		NewInput->MouseZ = 0; //TODO: Support mouse wheel
		Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0], GetKeyState(VK_LBUTTON) & (1<<15));
		Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1], GetKeyState(VK_MBUTTON) & (1<<15));
		Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2], GetKeyState(VK_RBUTTON) & (1<<15));
		Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3], GetKeyState(VK_XBUTTON1) & (1<<15));
		Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4], GetKeyState(VK_XBUTTON2) & (1<<15));

      // Controller input
      DWORD MaxControllerCount = XUSER_MAX_COUNT;
      if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
      {
        MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
      }

      for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex)
      {
        DWORD OurControllerIndex = ControllerIndex + 1;
        game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
        game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

        XINPUT_STATE ControllerState;
        if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
        {
          NewController->IsConnected = 1;
		  NewController->IsAnalog = OldController->IsAnalog;

          // NOTE(casey): This controller is plugged in
          // TODO: See if ControllerState.dwPacketNumber increments too rapidly
          XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

          // TODO: This is a square deadzone, check XInput to
          // verify that the deadzone is "round" and show how to do
          // round deadzone processing.
          NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
          NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
          if ((NewController->StickAverageX != 0.0f) || (NewController->StickAverageY != 0.0f))
          {
            NewController->IsAnalog = 1;
          }

          if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
          {
            NewController->StickAverageY = 1.0f;
            NewController->IsAnalog = 0;
          }

          if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
          {
            NewController->StickAverageY = -1.0f;
            NewController->IsAnalog = 0;
          }

          if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
          {
            NewController->StickAverageX = -1.0f;
            NewController->IsAnalog = 0;
          }

          if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
          {
            NewController->StickAverageX = 1.0f;
            NewController->IsAnalog = 0;
          }

          real32 Threshold = 0.5f;
          Win32ProcessXInputDigitalButton((NewController->StickAverageX < -Threshold) ? 1 : 0, &OldController->MoveLeft, 1, &NewController->MoveLeft);
          Win32ProcessXInputDigitalButton((NewController->StickAverageX > Threshold) ? 1 : 0, &OldController->MoveRight, 1, &NewController->MoveRight);
          Win32ProcessXInputDigitalButton((NewController->StickAverageY < -Threshold) ? 1 : 0, &OldController->MoveDown, 1, &NewController->MoveDown);
          Win32ProcessXInputDigitalButton((NewController->StickAverageY > Threshold) ? 1 : 0, &OldController->MoveUp, 1, &NewController->MoveUp);

          Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionDown, XINPUT_GAMEPAD_A, &NewController->ActionDown);
          Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionRight, XINPUT_GAMEPAD_B, &NewController->ActionRight);
          Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionLeft, XINPUT_GAMEPAD_X, &NewController->ActionLeft);
          Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionUp, XINPUT_GAMEPAD_Y, &NewController->ActionUp);
          Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
          Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);

          Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Start, XINPUT_GAMEPAD_START, &NewController->Start);
          Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back, XINPUT_GAMEPAD_BACK, &NewController->Back);
        } else
        {
          // NOTE(casey): The controller is not available
          NewController->IsConnected = 0;
        }
      }

      // Populate Screen Buffer to give to game
      game_offscreen_buffer GameBuffer = {};
      GameBuffer.Height = GlobalBackBuffer.Height;
      GameBuffer.Memory = GlobalBackBuffer.Memory;
      GameBuffer.Width = GlobalBackBuffer.Width;
      GameBuffer.Pitch = GlobalBackBuffer.Pitch;
      GameBuffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;

	  thread_context Thread = {};

	  if(Win32State.InputRecordingIndex)
	  {
		  Win32RecordInput(&Win32State, NewInput);
	  }
	  if(Win32State.InputPlayingIndex)
	  {
		  Win32PlaybackInput(&Win32State, NewInput);
	  }
      // GAME CODE ENTRYPOINT
	  if(Game.UpdateAndRender)
	  {
		Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &GameBuffer);
	  } else
	  {
		  //TODO: Logging
	  }

      // Game Sound
      // NOTE: Compute how much sound to write and where

      LARGE_INTEGER AudioWallClock = Win32GetWallClock();
      real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

      DWORD PlayCursor;
      DWORD WriteCursor;
      if (GlobalSecondaryBuffer->lpVtbl->GetCurrentPosition(GlobalSecondaryBuffer, &PlayCursor, &WriteCursor) == DS_OK)
      {
        /* NOTE(casey):

           Here is how sound output computation works.

           We define a safety value that is the number
           of samples we think our game update loop
           may vary by (let's say up to 2ms)

           When we wake up to write audio, we will look
           and see what the play cursor position is and we
           will forecast ahead where we think the play
           cursor will be on the next frame boundary.

           We will then look to see if the write cursor is
           before that by at least our safety value.  If
           it is, the target fill position is that frame
           boundary plus one frame.  This gives us perfect
           audio sync in the case of a card that has low
           enough latency.

           If the write cursor is _after_ that safety
           margin, then we assume we can never sync the
           audio perfectly, so we will write one frame's
           worth of audio plus the safety margin's worth
           of guard samples.
        */
        if (!SoundIsValid)
        {
          SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
          SoundIsValid = 1;
        }

        DWORD ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize);

        DWORD ExpectedSoundBytesPerFrame = (int)((real32)(SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz);
        real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
        DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) * (real32)ExpectedSoundBytesPerFrame);

        DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;

        DWORD SafeWriteCursor = WriteCursor;
        if (SafeWriteCursor < PlayCursor)
        {
          SafeWriteCursor += SoundOutput.SecondaryBufferSize;
        }
        Assert(SafeWriteCursor >= PlayCursor);
        SafeWriteCursor += SoundOutput.SafetyBytes;

        bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

        DWORD TargetCursor = 0;
        if (AudioCardIsLowLatency)
        {
          TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
        } else
        {
          TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes);
        }
        TargetCursor = (TargetCursor % SoundOutput.SecondaryBufferSize);

        DWORD BytesToWrite = 0;
        if (ByteToLock > TargetCursor)
        {
          BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
          BytesToWrite += TargetCursor;
        } else
        {
          BytesToWrite = TargetCursor - ByteToLock;
        }

        game_sound_output_buffer SoundBuffer = {};
        SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
        SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
        SoundBuffer.Samples = Samples;
		if(Game.GetSoundSamples)
		{
			Game.GetSoundSamples(&Thread, &GameMemory, &SoundBuffer);
		} else
		{
			//TODO: Logging
		}

#if INTERNAL
        win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
        Marker->OutputPlayCursor = PlayCursor;
        Marker->OutputWriteCursor = WriteCursor;
        Marker->OutputLocation = ByteToLock;
        Marker->OutputByteCount = BytesToWrite;
        Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

        DWORD UnwrappedWriteCursor = WriteCursor;
        if (UnwrappedWriteCursor < PlayCursor)
        {
          UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
        }
        AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
        AudioLatencySeconds = (((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) / (real32)SoundOutput.SamplesPerSecond);

#if 0
        char TextBuffer[256];
        _snprintf_s(TextBuffer, sizeof(TextBuffer), "BTL:%u TC:%u BTW:%u - PC:%u WC:%u DELTA:%u (%fs)\n", ByteToLock, TargetCursor, BytesToWrite, PlayCursor, WriteCursor, AudioLatencyBytes,
                    AudioLatencySeconds);
        OutputDebugStringA(TextBuffer);
#endif
#endif
        Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
      } else
      {
        SoundIsValid = 0;
      }

      // TODO: assert that the Region1Size/Region2Size is valid

      // Sleep until frame flip
      LARGE_INTEGER WorkCounter = Win32GetWallClock();
      real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);
      real32 SecondsElapsedForFrame = WorkSecondsElapsed;
      if (SecondsElapsedForFrame < TargetSecondsPerFrame)
      {
        if (SleepIsGranular)
        {
          DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
          if (SleepMS > 0)
          {
            Sleep(SleepMS);
          }
        }

        real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
        if (TestSecondsElapsedForFrame < TargetSecondsPerFrame)
          ;
        {
          // TODO: Log Missed Sleep Here
        }

        while (SecondsElapsedForFrame < TargetSecondsPerFrame)
        {
          SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
        }
      } else
      {
        // TODO(casey): MISSED FRAME RATE!
        // TODO(casey): Logging
      }

      LARGE_INTEGER EndCounter = Win32GetWallClock();
      real32 MSPerFrame = (1000.0f) * Win32GetSecondsElapsed(LastCounter, EndCounter);
      LastCounter = EndCounter;

      // Draw Display Buffer
      // TODO: Understand this code a little better...
      win32_window_dimension Dimension = Win32GetWindowDimension(WindowHandle);
      // NOTE: BackBuffer contains the data from the GameBuffer that is modified in the GameLogic
  		HDC DeviceContext = GetDC(WindowHandle);
      Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
	  ReleaseDC(WindowHandle, DeviceContext);


      FlipWallClock = Win32GetWallClock();

#if INTERNAL
      // NOTE(casey): This is debug code
      {
        if (GlobalSecondaryBuffer->lpVtbl->GetCurrentPosition(GlobalSecondaryBuffer, &PlayCursor, &WriteCursor) == DS_OK)
        {
          Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
          win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
          Marker->FlipPlayCursor = PlayCursor;
          Marker->FlipWriteCursor = WriteCursor;
        }
      }
#endif

      game_input *Temp = NewInput;
      NewInput = OldInput;
      OldInput = Temp;

      // Performance Info

#if 0
      uint64 EndCycleCount = __rdtsc();
      int64 CyclesElapsed = EndCycleCount - LastCycleCount;
      LastCycleCount = EndCycleCount;

      real32 FPS = 0.0f;
      real32 MCPF = (real32)(CyclesElapsed / (1000.0f * 1000.0f));

      // TODO: Better Logging situation.
      char str[256];
      sprintf_s(str, "%.02f ms/f,  %.02f FPS,  %.02f mc/f \n", MSPerFrame, FPS, MCPF);
      OutputDebugStringA(str);
#endif

#if INTERNAL
      ++DebugTimeMarkerIndex;
      if (DebugTimeMarkerIndex >= ArrayCount(DebugTimeMarkers))
      {
        DebugTimeMarkerIndex = 0;
      }
#endif
    }
  }

  return 0;
}
