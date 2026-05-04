// Empêche l'ouverture d'un terminal sur Windows en mode release
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

fn main() {
    open_bingo_lib::run();
}
