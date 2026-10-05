// Included after native_disc_image.c/native_assets.c in the native unity build.
// The importer uses the same disc parser, before any game/audio threads start.
#if (defined(_WIN32) || defined(__linux__)) && !defined(__EMSCRIPTEN__) && !defined(__vita__)

struct NativeDiscSetupTask
{
	SDL_AtomicInt done;
	SDL_AtomicInt cancelled;
	SDL_AtomicInt phase;
	SDL_AtomicInt progress;
	char *source;
	char destination[NATIVE_ASSETS_PATH_MAX];
	char error[2048];
	int result;
	int font;
};

internal u32 NativeDiscSetup_ReadBE32(const u8 *bytes)
{
	return ((u32)bytes[0] << 24) | ((u32)bytes[1] << 16) | ((u32)bytes[2] << 8) | bytes[3];
}

internal int NativeDiscSetup_ValidateFont(const char *path, struct NativeDiscSetupTask *task)
{
	SDL_IOStream *stream = SDL_IOFromFile(path, "rb");
	Sint64 size = stream != NULL ? SDL_GetIOSize(stream) : -1;
	u8 *bytes = NULL;
	int ok = 0;
	static const char *tables[] = {"cmap", "glyf", "head", "hhea", "hmtx", "loca", "maxp"};
	static const u32 minimumSizes[] = {4, 1, 54, 36, 4, 4, 6};
	u32 found = 0;
	if (size < 12 || size > 16 * 1024 * 1024 || (bytes = (u8 *)SDL_malloc((size_t)size)) == NULL || SDL_ReadIO(stream, bytes, (size_t)size) != (size_t)size)
	{
		goto done;
	}
	if (NativeDiscSetup_ReadBE32(bytes) != 0x00010000 && memcmp(bytes, "true", 4) != 0)
	{
		goto done;
	}
	u32 count = ((u32)bytes[4] << 8) | bytes[5];
	if (count == 0 || count > 128 || 12u + count * 16u > (u64)size)
	{
		goto done;
	}
	for (u32 i = 0; i < count; i++)
	{
		const u8 *entry = bytes + 12u + i * 16u;
		u32 offset = NativeDiscSetup_ReadBE32(entry + 8);
		u32 length = NativeDiscSetup_ReadBE32(entry + 12);
		if ((u64)offset + length > (u64)size)
		{
			goto done;
		}
		for (u32 table = 0; table < sizeof(tables) / sizeof(tables[0]); table++)
		{
			if (memcmp(entry, tables[table], 4) == 0 && length >= minimumSizes[table])
			{
				found |= 1u << table;
			}
		}
	}
	ok = found == (1u << (sizeof(tables) / sizeof(tables[0]))) - 1u;
done:
	if (stream != NULL)
	{
		SDL_CloseIO(stream);
	}
	SDL_free(bytes);
	if (!ok)
	{
		SDL_strlcpy(
		    task->error,
		    "This is not a readable TrueType font with the required font tables.\n\n"
		    "Extract the Crash-a-Like download and select the TTF itself, not the ZIP. Follow README > First-time setup > Add the optional Crash-a-Like font.",
		    sizeof(task->error));
	}
	return ok;
}

internal int NativeDiscSetup_FindRequiredFile(const char *path, struct NativeDiscImageFile *file, int raw)
{
	return NativeDiscImage_FindFile(path, file) && file->size != 0 &&
	       (u64)file->lba + (raw ? NativeDiscImage_RawSectorCount(file->size) : NativeDiscImage_DataSectorCount(file->size)) <= s_nativeDiscImageSectorCount;
}

internal int NativeDiscSetup_ValidateImage(const char *path, struct NativeDiscSetupTask *task)
{
	static const char *required[] = {"SYSTEM.CNF", "SCUS_944.26", "BIGFILE.BIG", "SOUNDS/KART.HWL", "TEST.STR", "XA/ENG.XNF"};
	static const char *xaDirs[] = {"XA/MUSIC", "XA/ENG/EXTRA", "XA/ENG/GAME"};
	struct NativeDiscImageFile file;
	u8 sector[NATIVE_DISC_IMAGE_RAW_SECTOR_SIZE];
	u8 *manifest = NULL;
	int ok = 0;

	SDL_SetAtomicInt(&task->progress, 0);
	if (!NativeDiscImage_InitPath(path))
	{
		SDL_snprintf(task->error, sizeof(task->error),
		             "This is not a readable raw MODE2/2352 disc image.\n\nChoose the NTSC-U Crash Team Racing BIN file, not a CUE, ZIP, or 2048-byte ISO.");
		goto done;
	}
	if (!NativeDiscImage_ReadRawSector(16, sector) ||
	    NativeDiscImage_ReadLE32(sector + NATIVE_DISC_IMAGE_FORM1_DATA_OFFSET + 80) > s_nativeDiscImageSectorCount)
	{
		SDL_snprintf(task->error, sizeof(task->error), "The disc image is truncated or has an invalid volume size.");
		goto done;
	}
	for (u32 i = 0; i < sizeof(required) / sizeof(required[0]); i++)
	{
		if (!NativeDiscSetup_FindRequiredFile(required[i], &file, i == 4))
		{
			SDL_snprintf(task->error, sizeof(task->error),
			             "The image is missing or has an incomplete %s.\n\nUse the NTSC-U (US) retail Crash Team Racing disc.", required[i]);
			goto done;
		}
	}
	if (!NativeDiscImage_FindFile("SCUS_944.26", &file) || file.size < 2048 || !NativeDiscImage_ReadDataBytes(&file, 0, sector, 8) ||
	    memcmp(sector, "PS-X EXE", 8) != 0)
	{
		SDL_snprintf(task->error, sizeof(task->error), "The NTSC-U game executable is invalid.");
		goto done;
	}
	if (!NativeDiscImage_FindFile("SYSTEM.CNF", &file) || file.size >= sizeof(sector) || !NativeDiscImage_ReadDataBytes(&file, 0, sector, file.size))
	{
		SDL_snprintf(task->error, sizeof(task->error), "The disc's boot configuration is invalid.");
		goto done;
	}
	sector[file.size] = 0;
	if (SDL_strstr((const char *)sector, "SCUS_944.26") == NULL)
	{
		SDL_snprintf(task->error, sizeof(task->error), "This disc does not boot the supported NTSC-U Crash Team Racing executable (SCUS_944.26).");
		goto done;
	}

	// Validate the XA manifest and every audio file it references, without using
	// extracted overrides from the player's existing installation.
	if (!NativeDiscImage_FindFile("XA/ENG.XNF", &file) || file.size < 0x44 || file.size > 1024u * 1024u || (manifest = (u8 *)malloc(file.size)) == NULL ||
	    !NativeDiscImage_ReadDataBytes(&file, 0, manifest, file.size) || NativeDiscImage_ReadLE32(manifest) != 0x464e4958 ||
	    NativeDiscImage_ReadLE32(manifest + 4) != 102 || NativeDiscImage_ReadLE32(manifest + 8) != 3)
	{
		SDL_snprintf(task->error, sizeof(task->error), "The disc's XA audio manifest is invalid or unreadable.");
		goto done;
	}
	u32 tracks = NativeDiscImage_ReadLE32(manifest + 0x10);
	u64 entries = 0x44u + (u64)NativeDiscImage_ReadLE32(manifest + 0x0c) * 4u;
	if (entries + (u64)tracks * 4u > file.size)
	{
		SDL_snprintf(task->error, sizeof(task->error), "The disc's XA audio table is incomplete.");
		goto done;
	}
	for (u32 category = 0; category < 3; category++)
	{
		u32 songs = NativeDiscImage_ReadLE32(manifest + 0x2c + category * 4);
		u32 first = NativeDiscImage_ReadLE32(manifest + 0x38 + category * 4);
		if ((u64)first + songs > tracks)
		{
			SDL_snprintf(task->error, sizeof(task->error), "The disc's XA audio ranges are invalid.");
			goto done;
		}
		for (u32 song = 0; song < songs; song++)
		{
			char audioPath[128];
			SDL_snprintf(audioPath, sizeof(audioPath), "%s/S%02u.XA", xaDirs[category], manifest[entries + ((u64)first + song) * 4u + 1u]);
			if (!NativeDiscSetup_FindRequiredFile(audioPath, &file, 1))
			{
				SDL_snprintf(task->error, sizeof(task->error), "The disc has a missing or incomplete audio file: %s", audioPath);
				goto done;
			}
		}
	}

	// Read the whole image so unreadable/truncated sectors cannot be installed.
	// This checks structure and required assets, not a retail checksum match.
	for (u32 lba = 0; lba < s_nativeDiscImageSectorCount; lba++)
	{
		if (SDL_GetAtomicInt(&task->cancelled))
		{
			goto done;
		}
		if (!NativeDiscImage_ReadRawSector(lba, sector) || memcmp(sector + 16, sector + 20, 4) != 0)
		{
			SDL_snprintf(task->error, sizeof(task->error), "The disc has an unreadable or invalid MODE2 sector at sector %u.", lba);
			goto done;
		}
		if ((lba & 255u) == 0)
		{
			SDL_SetAtomicInt(&task->progress, (int)((u64)lba * 100u / s_nativeDiscImageSectorCount));
		}
	}
	ok = 1;
done:
	free(manifest);
	NativeDiscImage_InitPath(NULL);
	return ok;
}

internal int SDLCALL NativeDiscSetup_ImportThread(void *userdata)
{
	struct NativeDiscSetupTask *task = (struct NativeDiscSetupTask *)userdata;
	char temporary[NATIVE_ASSETS_PATH_MAX];
	char directory[NATIVE_ASSETS_PATH_MAX];
	temporary[0] = 0;
	SDL_SetAtomicInt(&task->phase, 1);
	if (!(task->font ? NativeDiscSetup_ValidateFont(task->source, task) : NativeDiscSetup_ValidateImage(task->source, task)) ||
	    SDL_GetAtomicInt(&task->cancelled))
	{
		goto done;
	}
	if (!NativePath_Parent(directory, sizeof(directory), NativeStr8_FromCString(task->destination)) || !SDL_CreateDirectory(directory))
	{
		SDL_snprintf(task->error, sizeof(task->error), "Cannot create the destination folder for:\n%s\n\n%s", task->destination, SDL_GetError());
		goto done;
	}
	if (SDL_snprintf(temporary, sizeof(temporary), "%s/.turbocharged-import-%llu.tmp", directory, (unsigned long long)SDL_GetTicksNS()) >=
	    (int)sizeof(temporary))
	{
		temporary[0] = 0;
		SDL_snprintf(task->error, sizeof(task->error), "The assets folder path is too long.");
		goto done;
	}
	SDL_SetAtomicInt(&task->phase, 2);
	if (!SDL_CopyFile(task->source, temporary))
	{
		SDL_snprintf(task->error, sizeof(task->error), "Could not copy the %s. Check free space and folder permissions.\n\n%s",
		             task->font ? "font" : "disc image", SDL_GetError());
		goto done;
	}
	if (SDL_GetAtomicInt(&task->cancelled))
	{
		goto done;
	}
	SDL_SetAtomicInt(&task->phase, 3);
	if (!(task->font ? NativeDiscSetup_ValidateFont(temporary, task) : NativeDiscSetup_ValidateImage(temporary, task)) || SDL_GetAtomicInt(&task->cancelled))
	{
		goto done;
	}
	if (!SDL_RenamePath(temporary, task->destination))
	{
		SDL_snprintf(task->error, sizeof(task->error), "Could not install the %s at:\n%s\n\n%s", task->font ? "font" : "disc image", task->destination,
		             SDL_GetError());
		goto done;
	}
	task->result = 1;
done:
	if (temporary[0] != 0)
	{
		SDL_RemovePath(temporary);
	}
	SDL_SetAtomicInt(&task->done, 1);
	return 0;
}

internal void SDLCALL NativeDiscSetup_FileSelected(void *userdata, const char *const *files, int filter)
{
	struct NativeDiscSetupTask *task = (struct NativeDiscSetupTask *)userdata;
	(void)filter;
	if (files == NULL)
	{
		SDL_snprintf(task->error, sizeof(task->error), "Could not open the file picker:\n%s\n\nFollow the README to copy your %s manually to:\n%s",
		             SDL_GetError(), task->font ? "TTF file" : "BIN file", task->destination);
	}
	else if (files[0] != NULL)
	{
		task->source = SDL_strdup(files[0]);
		if (task->source == NULL)
		{
			SDL_snprintf(task->error, sizeof(task->error), "Not enough memory to select the file.");
		}
	}
	SDL_SetAtomicInt(&task->done, 1);
}

internal void NativeDiscSetup_Wait(struct NativeDiscSetupTask *task, SDL_Window *window, SDL_Renderer *renderer)
{
	while (!SDL_GetAtomicInt(&task->done))
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
			    (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE))
			{
				SDL_SetAtomicInt(&task->cancelled, 1);
			}
		}
		int phase = SDL_GetAtomicInt(&task->phase);
		const char *status = phase == 1   ? "Checking selected disc image..."
		                     : phase == 2 ? "Copying disc image..."
		                     : phase == 3 ? "Checking installed copy..."
		                                  : "Choose your NTSC-U Crash Team Racing BIN file.";
		if (task->font)
		{
			status = phase == 1   ? "Checking selected font..."
			         : phase == 2 ? "Copying font..."
			         : phase == 3 ? "Checking installed font..."
			                      : "Choose the Crash-a-Like TTF file.";
		}
		SDL_SetWindowTitle(window, status);
		if (renderer != NULL)
		{
			SDL_SetRenderDrawColor(renderer, 25, 29, 40, 255);
			SDL_RenderClear(renderer);
			SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
			SDL_RenderDebugText(renderer, 16, 24, "TURBOCHARGED - FIRST-TIME SETUP");
			SDL_RenderDebugText(renderer, 16, 56, status);
			if (!task->font && (phase == 1 || phase == 3))
			{
				SDL_RenderDebugTextFormat(renderer, 16, 80, "%d%%", SDL_GetAtomicInt(&task->progress));
			}
			SDL_RenderDebugText(renderer, 16, 112,
			                    SDL_GetAtomicInt(&task->cancelled) ? "Cancelling. Close the file picker if it is open." : "Your original file will be kept.");
			SDL_RenderPresent(renderer);
		}
		SDL_Delay(16);
	}
}

internal int NativeDiscSetup_Confirm(SDL_Window *window, const char *message, const char *accept)
{
	SDL_MessageBoxButtonData buttons[] = {{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"}, {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, accept}};
	SDL_MessageBoxData box = {SDL_MESSAGEBOX_INFORMATION, window, "Turbocharged setup", message, 2, buttons, NULL};
	int chosen = 0;
	return SDL_ShowMessageBox(&box, &chosen) && chosen == 1;
}

internal void NativeDiscSetup_Font(SDL_Window *window, SDL_Renderer *renderer)
{
	static const SDL_DialogFileFilter filters[] = {{"TrueType font (TTF)", "ttf"}, {"All files", "*"}};
	char destination[NATIVE_ASSETS_PATH_MAX];
	char existing[NATIVE_ASSETS_PATH_MAX];
	char message[2048];
	struct NativeDiscSetupTask check = {0};
	if (!NativeAssets_BuildPath("fonts/crash-a-like.ttf", destination, sizeof(destination)))
	{
		return;
	}
	if (NativeAssets_ResolvePath("fonts/crash-a-like.ttf", existing, sizeof(existing)) && NativeDiscSetup_ValidateFont(existing, &check))
	{
		return;
	}
	SDL_snprintf(message, sizeof(message),
	             "Luckiest Guy is already included as a menu/HUD font. No extra download is needed.\n\n"
	             "You can also add the optional Crash-a-Like font. "
	             "If you already have it, choose the extracted TTF file. It will be checked and copied to:\n%s\n\n"
	             "If you do not have it, follow README > First-time setup > Add the optional Crash-a-Like font for download and setup instructions.\n\n"
	             "Choose Use Luckiest Guy to continue with the bundled font. The original game font is also available in Options. "
	             "Fuzzy Bubbles for the title lettering is already included.",
	             destination);
	SDL_MessageBoxButtonData buttons[] = {{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Use Luckiest Guy"},
	                                      {0, 1, "Open README"},
	                                      {0, 2, "Choose Crash-a-Like"}};
	SDL_MessageBoxData box = {SDL_MESSAGEBOX_INFORMATION, window, "Optional Crash-a-Like font", message, 3, buttons, NULL};
	int chosen = 0;
	if (!SDL_ShowMessageBox(&box, &chosen))
	{
		return;
	}
	if (chosen == 0)
	{
		gNativeFont = NATIVE_FONT_LUCKIEST_GUY;
		return;
	}
	if (chosen == 1)
	{
		if (!SDL_OpenURL("https://github.com/CameronRedmore/Crash-Team-Racing-Turbocharged#3-add-the-optional-crash-a-like-font"))
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Font setup instructions",
			                         "Open the project's README on GitHub and follow First-time setup > Add the optional Crash-a-Like font.\n\n"
			                         "https://github.com/CameronRedmore/Crash-Team-Racing-Turbocharged",
			                         window);
		}
		return;
	}
	for (;;)
	{
		struct NativeDiscSetupTask task = {0};
		task.font = 1;
		SDL_strlcpy(task.destination, destination, sizeof(task.destination));
		SDL_ShowOpenFileDialog(NativeDiscSetup_FileSelected, &task, window, filters, 2, NULL, false);
		NativeDiscSetup_Wait(&task, window, renderer);
		if (task.error[0] != 0)
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Font setup unavailable", task.error, window);
			return;
		}
		if (task.source == NULL || SDL_GetAtomicInt(&task.cancelled))
		{
			SDL_free(task.source);
			return;
		}
		SDL_PathInfo info;
		if (SDL_GetPathInfo(destination, &info) &&
		    !NativeDiscSetup_Confirm(window, "A font already exists in the assets folder. Replace it with the selected font after validation?", "Replace font"))
		{
			SDL_free(task.source);
			return;
		}
		SDL_SetAtomicInt(&task.done, 0);
		SDL_Thread *thread = SDL_CreateThread(NativeDiscSetup_ImportThread, "font-import", &task);
		if (thread == NULL)
		{
			SDL_snprintf(task.error, sizeof(task.error), "Could not start font import: %s", SDL_GetError());
		}
		else
		{
			NativeDiscSetup_Wait(&task, window, renderer);
			SDL_WaitThread(thread, NULL);
		}
		SDL_free(task.source);
		if (task.result)
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Font installed",
			                         "Crash-a-Like has been copied into the assets folder.\n\nSelect Options > Interface > Font > CRASH-A-LIKE to use it.",
			                         window);
			return;
		}
		if (SDL_GetAtomicInt(&task.cancelled))
		{
			return;
		}
		SDL_snprintf(message, sizeof(message), "%s\n\nChoose another TTF file?", task.error);
		if (!NativeDiscSetup_Confirm(window, message, "Try again"))
		{
			return;
		}
	}
}

internal int NativeDiscSetup_Run(int needDisc)
{
	static const SDL_DialogFileFilter filters[] = {{"Raw disc image (BIN)", "bin"}, {"All files", "*"}};
	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;
	char baseDir[NATIVE_ASSETS_PATH_MAX];
	char destination[NATIVE_ASSETS_PATH_MAX];
	char message[2048];
	int result = !needDisc;
	if (!needDisc)
	{
		struct NativeDiscSetupTask check = {0};
		char existing[NATIVE_ASSETS_PATH_MAX];
		if (NativeAssets_ResolvePath("fonts/crash-a-like.ttf", existing, sizeof(existing)) && NativeDiscSetup_ValidateFont(existing, &check))
		{
			return 1;
		}
	}
	SDL_strlcpy(baseDir, NativeAssets_GetBaseDir(), sizeof(baseDir));
	if (!NativeAssets_BuildPath("ctr-u.bin", destination, sizeof(destination)) || !SDL_InitSubSystem(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "[Turbocharged] Disc setup unavailable: %s\n", SDL_GetError());
		return 0;
	}
	window = SDL_CreateWindow("Turbocharged setup", 520, 160, 0);
	if (window == NULL)
	{
		fprintf(stderr, "[Turbocharged] Disc setup window unavailable: %s\n", SDL_GetError());
		goto done;
	}
	renderer = SDL_CreateRenderer(window, NULL);
	if (!needDisc)
	{
		goto done;
	}
	SDL_snprintf(message, sizeof(message),
	             "Turbocharged needs your NTSC-U (US) retail Crash Team Racing disc image.\n\n"
	             "Choose a raw MODE2/2352 BIN file. It will be checked and copied to:\n%s\n\n"
	             "Your original file will be kept. A CUE, ZIP, or 2048-byte ISO will not work.",
	             destination);
	if (!NativeDiscSetup_Confirm(window, message, "Choose disc image"))
	{
		goto done;
	}
	for (;;)
	{
		struct NativeDiscSetupTask task = {0};
		SDL_strlcpy(task.destination, destination, sizeof(task.destination));
		SDL_ShowOpenFileDialog(NativeDiscSetup_FileSelected, &task, window, filters, 2, NULL, false);
		NativeDiscSetup_Wait(&task, window, renderer);
		if (task.error[0] != 0)
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Disc setup unavailable", task.error, window);
			break;
		}
		if (task.source == NULL || SDL_GetAtomicInt(&task.cancelled))
		{
			SDL_free(task.source);
			break;
		}
		SDL_PathInfo info;
		if (SDL_GetPathInfo(destination, &info) &&
		    !NativeDiscSetup_Confirm(window, "An image already exists in the assets folder. Replace it with the selected image after validation?",
		                             "Replace image"))
		{
			SDL_free(task.source);
			break;
		}
		SDL_SetAtomicInt(&task.done, 0);
		SDL_Thread *thread = SDL_CreateThread(NativeDiscSetup_ImportThread, "disc-import", &task);
		if (thread == NULL)
		{
			SDL_snprintf(task.error, sizeof(task.error), "Could not start disc import: %s", SDL_GetError());
		}
		else
		{
			NativeDiscSetup_Wait(&task, window, renderer);
			SDL_WaitThread(thread, NULL);
		}
		SDL_free(task.source);
		// Restore the installed disc and invalidate the extracted-file index.
		NativeAssets_Init(baseDir);
		if (task.result && NativeAssets_Validate())
		{
			result = 1;
			break;
		}
		if (SDL_GetAtomicInt(&task.cancelled))
		{
			break;
		}
		if (task.error[0] == 0)
		{
			SDL_strlcpy(task.error, "Game assets could not be loaded after import. Check existing extracted overrides in the assets folder.",
			            sizeof(task.error));
		}
		SDL_snprintf(message, sizeof(message), "%s\n\nChoose another disc image?", task.error);
		if (!NativeDiscSetup_Confirm(window, message, "Try again"))
		{
			break;
		}
	}
done:
	if (result && window != NULL)
	{
		NativeDiscSetup_Font(window, renderer);
		NativeAssets_Init(baseDir);
	}
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return result;
}

#else
internal int NativeDiscSetup_Run(int needDisc)
{
	return !needDisc;
}
#endif
