#include "NuttyRemote.h"



static void nutty_main(void) {



}

NuttyAppDefinition NuttyRemote = {
    .appName = "Nutty Remote",
    .appMainEntry = nutty_main,
    .appHidden = false
};
