#include "NuttyAbout.h"



static void nutty_main(void) {



}

NuttyAppDefinition NuttyAbout = {
    .appName = "About",
    .appMainEntry = nutty_main,
    .appHidden = false
};
