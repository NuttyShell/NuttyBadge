#include "NuttyRF.h"



static void nutty_main(void) {



}

NuttyAppDefinition NuttyRF = {
    .appName = "Nutty RF",
    .appMainEntry = nutty_main,
    .appHidden = true
};
