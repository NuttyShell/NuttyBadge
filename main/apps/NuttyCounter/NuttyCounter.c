#include "NuttyCounter.h"



static void nutty_main(void) {



}

NuttyAppDefinition NuttyCounter = {
    .appName = "Counter",
    .appMainEntry = nutty_main,
    .appHidden = false
};
