#include "NuttyStopwatch.h"



static void nutty_main(void) {



}

NuttyAppDefinition NuttyStopwatch = {
    .appName = "Stopwatch",
    .appMainEntry = nutty_main,
    .appHidden = false
};
