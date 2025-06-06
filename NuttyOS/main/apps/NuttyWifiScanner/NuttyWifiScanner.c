#include "NuttyWifiScanner.h"



static void nutty_main(void) {



}

NuttyAppDefinition NuttyWifiScanner = {
    .appName = "Wifi Scanner",
    .appMainEntry = nutty_main,
    .appHidden = false
};
