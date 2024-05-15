#include "NuttyAudioPlayer.h"



static void nutty_main(void) {



}

NuttyAppDefinition NuttyAudioPlayer = {
    .appName = "Audio Player",
    .appMainEntry = nutty_main,
    .appHidden = false
};
