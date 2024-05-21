#include "NuttyBTRemote.h"



static void nutty_main(void) {



}

NuttyAppDefinition NuttyBTRemote = {
    .appName = "Bluetooth Remote",
    .appMainEntry = nutty_main,
    .appHidden = false
};
