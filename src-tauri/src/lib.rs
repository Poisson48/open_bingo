// Point d'entrée mobile (Android/iOS) — requis par Tauri v2
#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .invoke_handler({
            #[cfg(desktop)]
            { tauri::generate_handler![save_file_dialog] }
            #[cfg(mobile)]
            { tauri::generate_handler![] }
        })
        .run(tauri::generate_context!())
        .expect("erreur lors du démarrage de l'application");
}

/// Ouvre une dialog de sauvegarde native et écrit le fichier JSON choisi par l'utilisateur.
/// Retourne `true` si sauvegardé, `false` si annulé.
#[cfg(desktop)]
#[tauri::command]
async fn save_file_dialog(filename: String, content: String) -> Result<bool, String> {
    let result = tauri::async_runtime::spawn_blocking(move || {
        rfd::FileDialog::new()
            .set_file_name(&filename)
            .add_filter("JSON", &["json"])
            .save_file()
    })
    .await
    .map_err(|e| e.to_string())?;

    match result {
        Some(path) => {
            std::fs::write(&path, content.as_bytes()).map_err(|e| e.to_string())?;
            Ok(true)
        }
        None => Ok(false),
    }
}
