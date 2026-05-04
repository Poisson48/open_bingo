// Point d'entrée mobile (Android/iOS) — requis par Tauri v2
#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .run(tauri::generate_context!())
        .expect("erreur lors du démarrage de l'application");
}
