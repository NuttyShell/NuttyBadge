#include "NuttyRGBControl.h"



static void nutty_main(void) {



}

NuttyAppDefinition NuttyRGBControl = {
    .appName = "RGB Control",
    .appMainEntry = nutty_main,
    .appHidden = false
};
