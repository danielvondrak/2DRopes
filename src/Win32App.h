#if !defined(WIN32_H)

typedef struct win32_sound_output {
  int SamplesPerSecond;
  uint32 RunningSampleIndex;
  int BytesPerSample;
  DWORD SecondaryBufferSize;
  DWORD SafetyBytes;
  
}win32_sound_output;

typedef struct win32_window_dimension
{
    int Width;
    int Height;
}win32_window_dimension;

typedef struct win32_offscreen_buffer {

  BITMAPINFO Info; // buffer for display
  void *Memory;
  int Width;
  int Height;
  int Pitch;
  int BytesPerPixel;
}win32_offscreen_buffer;

typedef struct win32_debug_time_marker
{
	
	DWORD OutputPlayCursor;
	DWORD OutputWriteCursor;
	DWORD OutputLocation;
	DWORD OutputByteCount;
    DWORD ExpectedFlipPlayCursor;

	DWORD FlipPlayCursor;
	DWORD FlipWriteCursor;
}win32_debug_time_marker;

typedef struct win32_game_code {
  HMODULE GameCodeDLL;
  FILETIME DLLLastWriteTime;

  //NOTE: These callbacks can be 0!
  // Must check before calling them
  game_update_and_render *UpdateAndRender;
  game_get_sound_samples *GetSoundSamples;

  bool32 IsValid;
} win32_game_code;

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH

typedef struct win32_replay_buffer
{
	HANDLE FileHandle;
	HANDLE MemoryMap;
	char Filename[WIN32_STATE_FILE_NAME_COUNT];
	void *MemoryBlock;
}win32_replay_buffer;

typedef struct win32_state
{
	uint64 TotalSize;
	void *GameMemoryBlock;
	win32_replay_buffer ReplayBuffers[4];

	HANDLE RecordingHandle;
	int InputRecordingIndex;

	HANDLE PlaybackHandle;
	int InputPlayingIndex;


	char EXEFilename[WIN32_STATE_FILE_NAME_COUNT];
	char *OnePastLastEXEFilenameSlash;
}win32_state;

#define WIN32_H
#endif
