#include <db/sqlite3.h>
#include <const/constant.h>
#include <uiobjects/image.h>
#include <vector>
#include <iostream>

class AchievementDB {
private:
    sqlite3* db;

public:
    AchievementDB(const char* dbPath) {
        if (sqlite3_open(dbPath, &db) != SQLITE_OK) {
            std::cerr << "Error abriendo BDD: " << sqlite3_errmsg(db) << std::endl;
        }
        createTable();
    }

    ~AchievementDB() {
        sqlite3_close(db);
    }

    void createTable() {
        const char* sql = "CREATE TABLE IF NOT EXISTS achievements ("
                          "id INTEGER PRIMARY KEY, gameID INTEGER, badgeName TEXT, locked INTEGER, sectionType INTEGER, "
                          "points INTEGER, type INTEGER, badgeUrl TEXT, title TEXT, "
                          "description TEXT, progress TEXT, width INTEGER, height INTEGER, "
                          "pixels BLOB); "
						  "CREATE INDEX IF NOT EXISTS idx_game ON achievements(gameID);";
        sqlite3_exec(db, sql, NULL, NULL, NULL);
    }

    bool saveAchievement(const AchievementState& ach) {
        const char* sql = "INSERT OR REPLACE INTO achievements VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, ach.id);
		sqlite3_bind_int(stmt, 2, ach.gameId);
        sqlite3_bind_text(stmt, 3, ach.badgeName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, ach.locked ? 1 : 0);
        sqlite3_bind_int(stmt, 5, ach.sectionType);
        sqlite3_bind_int(stmt, 6, ach.points);
        sqlite3_bind_int(stmt, 7, (int)ach.type);
        sqlite3_bind_text(stmt, 8, ach.badgeUrl.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, ach.title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 10, ach.description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 11, ach.progress.c_str(), -1, SQLITE_TRANSIENT);

        if (ach.badge) {
            sqlite3_bind_int(stmt, 12, ach.badge->w);
            sqlite3_bind_int(stmt, 13, ach.badge->h);
            int size = ach.badge->h * ach.badge->pitch;
			sqlite3_bind_blob(stmt, 14, ach.badge->pixels, size, SQLITE_TRANSIENT);
        } else {
            for(int i=12; i<=14; i++) sqlite3_bind_null(stmt, i);
        }

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }

	bool updateStatus(uint32_t id, bool locked, uint8_t sectionType) {
		// Usamos UPDATE para modificar solo las columnas necesarias sin tocar el BLOB
		const char* sql = "UPDATE achievements SET locked = ?, sectionType = ? WHERE id = ?;";
		sqlite3_stmt* stmt;

		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
			return false;
		}

		sqlite3_bind_int(stmt, 1, locked ? 1 : 0);
		sqlite3_bind_int(stmt, 2, (int)sectionType);
		sqlite3_bind_int(stmt, 3, id);

		int res = sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		return (res == SQLITE_DONE);
	}

	bool updateAchievementsStatus(const std::vector<AchievementState>& achievements) {
		sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

		const char* sql = "UPDATE achievements SET locked = ?, sectionType = ? WHERE id = ?;";
		sqlite3_stmt* stmt;

		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

		for (size_t i = 0; i < achievements.size(); ++i) {
			// Acceso directo por objeto (usando el punto '.' en lugar de '->')
			const AchievementState& ach = achievements[i];
        
			sqlite3_bind_int(stmt, 1, ach.locked ? 1 : 0);
			sqlite3_bind_int(stmt, 2, (int)ach.sectionType);
			sqlite3_bind_int(stmt, 3, ach.id);

			sqlite3_step(stmt);
			sqlite3_reset(stmt);
		}

		sqlite3_finalize(stmt);
		sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
		return true;
	}

    std::vector<AchievementState> loadByGame(uint32_t gameID) {
		std::vector<AchievementState> list;
		// Filtramos por gameID en la consulta SQL
		const char* sql = "SELECT id, badgeName, locked, sectionType, points, type, "
						  "badgeUrl, title, description, progress, width, height, pixels "
						  "FROM achievements WHERE gameID = ?;";
		sqlite3_stmt* stmt;

		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int(stmt, 1, gameID);

			while (sqlite3_step(stmt) == SQLITE_ROW) {
				AchievementState ach; // Creamos el objeto (se usará el operador de copia al hacer push_back)
				ach.id          = sqlite3_column_int(stmt, 0);
				ach.badgeName   = (const char*)sqlite3_column_text(stmt, 1);
				ach.locked      = sqlite3_column_int(stmt, 2) != 0;
				ach.sectionType = (uint8_t)sqlite3_column_int(stmt, 3);
				ach.points      = sqlite3_column_int(stmt, 4);
				ach.type        = (ACH_TYPE)sqlite3_column_int(stmt, 5);
				ach.badgeUrl    = (const char*)sqlite3_column_text(stmt, 6);
				ach.title       = (const char*)sqlite3_column_text(stmt, 7);
				ach.description = (const char*)sqlite3_column_text(stmt, 8);
				ach.progress    = (const char*)sqlite3_column_text(stmt, 9);

				// Reconstruir imagen si existe
				if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) {
					int w = sqlite3_column_int(stmt, 10);
					int h = sqlite3_column_int(stmt, 11);
					const void* blob = sqlite3_column_blob(stmt, 12);
					int bytes = sqlite3_column_bytes(stmt, 12);

					ach.badge = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 16, 0xF800, 0x07E0, 0x001F, 0);
					if (ach.badge) {
						memcpy(ach.badge->pixels, blob, bytes);
						// Generamos la caché de gris inmediatamente
						//ach.badgeLocked = SDL_DisplayFormat(ach.badge);
						//if (ach.badgeLocked) Image::convertirGrises16Bits(ach.badgeLocked);
					}
				}
				list.push_back(ach); // Aquí se usa el constructor de copia que definimos antes
			}
		}
		sqlite3_finalize(stmt);
		return list;
	}

	AchievementState* getAchievement(uint32_t id) {
		const char* sql = "SELECT badgeName, gameID, locked, sectionType, points, type, badgeUrl, title, "
						  "description, progress, width, height, pixels "
						  "FROM achievements WHERE id = ? LIMIT 1;";
		sqlite3_stmt* stmt;
		AchievementState* ach = NULL;

		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int(stmt, 1, id);

			if (sqlite3_step(stmt) == SQLITE_ROW) {
				ach = new AchievementState();
				ach->id = id;

				// Mapeo de columnas (empezando desde 0 según el SELECT)
				ach->badgeName    = (const char*)sqlite3_column_text(stmt, 0);
				ach->gameId       = sqlite3_column_int(stmt, 1) != 0;
				ach->locked       = sqlite3_column_int(stmt, 2) != 0;
				ach->sectionType  = (uint8_t)sqlite3_column_int(stmt, 3);
				ach->points       = sqlite3_column_int(stmt, 4);
				ach->type         = (ACH_TYPE)sqlite3_column_int(stmt, 5);
				ach->badgeUrl     = (const char*)sqlite3_column_text(stmt, 6);
				ach->title        = (const char*)sqlite3_column_text(stmt, 7);
				ach->description  = (const char*)sqlite3_column_text(stmt, 8);
				ach->progress     = (const char*)sqlite3_column_text(stmt, 9);

				// Reconstrucción de la imagen (BLOB)
				if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) {
					int w = sqlite3_column_int(stmt, 10);
					int h = sqlite3_column_int(stmt, 11);
					const void* blob = sqlite3_column_blob(stmt, 12);
					int bytes = sqlite3_column_bytes(stmt, 12);

					// Crear superficie 16 bits (RGB565)
					ach->badge = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 16, 0xF800, 0x07E0, 0x001F, 0);
					if (ach->badge) {
						memcpy(ach->badge->pixels, blob, bytes);
						// Generar la versión bloqueada para la caché de renderizado
						//ach->badgeLocked = SDL_DisplayFormat(ach->badge);
						//if (ach->badgeLocked) {
						//	Image::convertirGrises16Bits(ach->badgeLocked);
						//}
					}
				}
				// Inicializar estados que no están en BDD
				ach->isDownloading = false;
				ach->isSection = (ach->sectionType != 0); 
			}
		}

		sqlite3_finalize(stmt);
		return ach; // Retorna NULL si no se encuentra
	}

	bool exists(uint32_t id) {
		const char* sql = "SELECT 1 FROM achievements WHERE id = ? LIMIT 1;";
		sqlite3_stmt* stmt;
		bool found = false;

		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
			// Enlazar los parámetros de búsqueda
			sqlite3_bind_int(stmt, 1, id);

			// Si sqlite3_step devuelve SQLITE_ROW, es que existe al menos una fila
			if (sqlite3_step(stmt) == SQLITE_ROW) {
				found = true;
			}
		}

		sqlite3_finalize(stmt);
		return found;
	}
};