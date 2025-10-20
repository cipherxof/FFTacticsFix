# FFTacticsFix
A plugin that aims to provide additional options and fix issues with Final Fantasy Tactics: The Ivalice Chronicles on PC.

## Features
- [x] Remove grain filter
- [x] Play animated cutscene videos in place of the in-game cutscenes
- [x] Increased rendering resolution
- [x] Ultrawide support
- [ ] Increase framerate
      
## Installation
[Download](https://github.com/cipherxof/FFTacticsFix/releases) and extract the zip to the root of your game directory.
 
<img width="905" height="352" alt="image" src="https://github.com/user-attachments/assets/6b576ce9-44c6-4a45-821a-d6674286312d" />

## Configuration

scripts/FFTacticsFix.ini

```ini
[Settings]
RenderScale = 4 # 4 is the default. 10 is 4k
PreferMovies = 1 # Play movies instead of cutscenes
DisableFilter = 1 # Disable the grain filter
```

*Ultrawide users must launch their game in borderless windowed mode with your preferred ultrawide resolution. Changing resolutions after launch will not work**

## Steam Deck / Linux

Use the following launch options

```bash
 `WINEDLLOVERRIDES="wininet,winhttp=n,b" %command%` 
 ```
 
## Before/After (Filter Removed/Increased Resolution/Ultrawide)

![ultrawide](https://github.com/user-attachments/assets/4ff92e18-cf6e-4ef3-84ad-164fa4f05638)

