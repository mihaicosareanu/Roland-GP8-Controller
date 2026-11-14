
Import("env")

env.AddPostAction(
    "$BUILD_DIR/${PROGNAME}.elf",
    env.VerboseAction(
        '$OBJCOPY -O ihex "$TARGET" '
        '"$BUILD_DIR/${PROGNAME}.hex"',
        "Generating Intel HEX file"
    )
)
