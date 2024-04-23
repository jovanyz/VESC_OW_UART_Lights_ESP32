/* Define sequences for arduino-pixels */
// All sequence functions must follow this format
// mode_position is the position in the sequence, this needs is defined as long for the UNO WiFi which would otherwise be limited to (2^8)/2

typedef long (*pfnSequence)(long mode_position);

struct mode {
        char * mode_name;            // Lower case - no spaces 
        char * title;               // Value to show to user
        char * description;         // Tool tip (if applicable)
        int group;                  // Group together sequences (allows some to be excluded) 0 = basic, 1 = common, 2 = waves, 3 = particle effects, 4 = special
        pfnSequence seqFunction;    // ptr to function
};


// define all sequences here. All these parameters are required even if the sequence doesn't use them (eg. allOff does not use colors or num_colors)
long Default(long mode_position);
long Rainbow(long mode_position);
long ColorWipe(long mode_position);
long ColorFade(long mode_position);
long Lava(long mode_position);
long Canopy(long mode_position);
long Ocean(long mode_position);
long RollingWave(long mode_position);
long ColorWave(long mode_position);
long Fireflies(long mode_position);
long Confetti(long mode_position);
long Comet(long mode_position);
long PacMan(long mode_position);
long TailColor(long mode_position);
long PixelFinder(long mode_position);
long unused0(long mode_position);
long unused1(long mode_position);
long unused2(long mode_position);

// Add new sequences to this structure. This is used for web page to know what options are available
// The group argument can be used to group certain sequences together (eg. to only show certain sequences or display on a different tab)
struct mode modes[] {
    { .mode_name = "default", .title = "Default", .description = "Default OW Mode", .group = 0, .seqFunction = Default},
    { .mode_name = "rainbow", .title = "Rainbow", .description = "Basic Rainbow", .group = 1, .seqFunction = Rainbow },    
    { .mode_name = "colorwipe", .title = "Color Wipe", .description = "Random Color Wipe", .group = 1, .seqFunction = ColorWipe },
    { .mode_name = "colorfade", .title = "Color Fade", .description = "Random Color Fade", .group = 1, .seqFunction = ColorFade },
    { .mode_name = "lava", .title = "Lava", .description = "Reddish Flowing Wave, Like Lava", .group = 2, .seqFunction = Lava },
    { .mode_name = "canopy", .title = "Canopy", .description = "Green Flowing Wave, Like Treetops", .group = 2, .seqFunction = Canopy },
    { .mode_name = "ocean", .title = "Ocean", .description = "Blue Flowing Wave, Like Ocean", .group = 2, .seqFunction = Ocean },
    { .mode_name = "rollingwave", .title = "Rolling Wave", .description = "Color Changing Flowing Wave", .group = 2, .seqFunction = RollingWave },
    { .mode_name = "colorwave", .title = "Color Wave", .description = "Flowing Wave at User Color", .group = 2, .seqFunction = ColorWave },
    { .mode_name = "fireflies", .title = "Fireflies", .description = "Flickering Green Speckles", .group = 3, .seqFunction = Fireflies },
    { .mode_name = "confetti", .title = "Confetti", .description = "Multicolor Speckles", .group = 3, .seqFunction = Confetti },
    { .mode_name = "comet", .title = "Comet", .description = "Comet with Tail", .group = 4, .seqFunction = Comet },
    { .mode_name = "pacman", .title = "PacMan", .description = "Pacman Eats Faster as You Ride!", .group = 4, .seqFunction = PacMan },
    { .mode_name = "tailcolor", .title = "TailColor", .description = "Classic Headlights, Colored Tail", .group = 4, .seqFunction = TailColor },
    { .mode_name = "pixelfinder", .title = "PixelFinder", .description = "Use Brightness Slider to Select Pixel", .group = 4, .seqFunction = PixelFinder },
    { .mode_name = "empty", .title = "NA", .description = "unused", .group = 5, .seqFunction = unused0 },
    { .mode_name = "empty1", .title = "NA", .description = "unused", .group = 5, .seqFunction = unused1 },
    { .mode_name = "emtpty2", .title = "NA", .description = "unused", .group = 5, .seqFunction = unused2 }
};
