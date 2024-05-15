#include "NuttyTest.h"



static void nutty_main(void) {



}

NuttyAppDefinition NuttyTest = {
    .appName = "Test App",
    .appMainEntry = nutty_main,
    .appHidden = false
};
