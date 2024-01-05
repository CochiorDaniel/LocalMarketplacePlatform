#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h> 
#include <sqlite3.h>

/* portul folosit */
#define PORT 2909
#define MAX_COMMAND_LENGTH 1028

/* codul de eroare returnat de anumite apeluri */
extern int errno;

typedef struct thData{
	int idThread; //id-ul thread-ului tinut in evidenta de acest program
	int cl; //descriptorul intors de accept
}thData;

typedef struct {
    char command[50];
    char **params;      // Vector de șiruri pentru parametrii
    int numParams;  
} CommandParams;

static void *treat(void *); /* functia executata de fiecare thread ce realizeaza comunicarea cu clientii */
void raspunde(void *);

void freeCommandParams(CommandParams *cmdParams) {
    
    for (int i = 0; i < cmdParams->numParams; i++) {
        free(cmdParams->params[i]);
    }
    free(cmdParams->params);
}

CommandParams parseCommand(char *input) {
    CommandParams result;

    result.params = NULL;
    result.numParams = 0;

    char *token = strtok(input, ":");

    strcpy(result.command, token);

    while ((token = strtok(NULL, " ")) != NULL) {
        result.params = realloc(result.params, (result.numParams + 1) * sizeof(char*));
        result.params[result.numParams] = strdup(token);
        result.numParams++;
    }

    return result;
}

char* exit_f(int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    sqlite3_stmt *stmt_upd;
    sqlite3 *db;
    int open_db = sqlite3_open("mkDB.db", &db);
    if(open_db != SQLITE_OK){
        printf("Eroare la deschiderea bazei de date!\n");
        exit(EXIT_FAILURE);
    }
    int res_upd;

    char *query2 = sqlite3_mprintf("UPDATE Useri SET logged = 0 WHERE id = '%d';", *id);
    res_upd = sqlite3_prepare_v2(db, query2, -1, &stmt_upd, 0);
    sqlite3_step(stmt_upd);
    sqlite3_finalize(stmt_upd);
    sqlite3_close(db);
    strcpy(result, "exit");

    return result;
}

char* login_client(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    CommandParams params = parseCommand(cmd);
    if(params.numParams != 2){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "login: <username> <password>\n");
        freeCommandParams(&params);
        return result;
    }
    
    params.params[1] = strtok(params.params[1], "\n");
    if(*id != -1){
        strcat(result, "Sunteti deja logat!\n");
        freeCommandParams(&params);
        return result;
    }
    else{
        sqlite3_stmt *stmt, *stmt_upd;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_db, res_upd;
        char *query1 = sqlite3_mprintf("SELECT * FROM Useri WHERE username = '%q' AND password = '%q';", params.params[0], params.params[1]);
        // printf("%s\n", query1);
        res_db = sqlite3_prepare_v2(db, query1, -1, &stmt, 0);
        if(res_db != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
            exit(EXIT_FAILURE);
        }
        else{
            if(sqlite3_step(stmt) == SQLITE_ROW){
                *id = sqlite3_column_int(stmt, 0);
                char *query2 = sqlite3_mprintf("UPDATE Useri SET logged = 1 WHERE id = '%d';", *id);
                res_upd = sqlite3_prepare_v2(db, query2, -1, &stmt_upd, 0);
                sqlite3_step(stmt_upd);
                sqlite3_finalize(stmt_upd);
                strcat(result, "V-ati logat cu succes!\n");
            }
            else{
                strcat(result, "Login esuat!\n");
            }
        }
        printf("id = %d\n", *id);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    freeCommandParams(&params);
    return result;
}

char* creare_cont(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }
    CommandParams params = parseCommand(cmd);
    if(params.numParams != 3){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "creare cont: <username> <password> <sold>\n");
        freeCommandParams(&params);
        return result;
    }

    if(*id != -1){
        strcat(result, "Sunteti deja logat!\n");
        freeCommandParams(&params);
        return result;
    }
    else{
        sqlite3_stmt *stmt_insert;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_insert;
        char* query_inset = sqlite3_mprintf("INSERT INTO Useri (username, password, sold, logged) VALUES ('%q', '%q', '%q', 0);", params.params[0], params.params[1], params.params[2]);
        res_insert = sqlite3_prepare_v2(db, query_inset, -1, &stmt_insert, 0);
        sqlite3_step(stmt_insert);
        sqlite3_finalize(stmt_insert);
        strcat(result, "Contul a fost creat cu succes!\n");
        sqlite3_close(db);   
    }

    freeCommandParams(&params);
    return result;
}

char* lista_produse(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    if(*id == -1){
        strcpy(result, "Nu sunteti logat!\n");
        return result;
    }
    else{
        sqlite3_stmt *stmt;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_db;
        char *query1 = sqlite3_mprintf("SELECT * FROM Oferte WHERE buyer_id = -1;");
        res_db = sqlite3_prepare_v2(db, query1, -1, &stmt, 0);
        if(res_db != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
            exit(EXIT_FAILURE);
        }
        else{
            while(sqlite3_step(stmt) == SQLITE_ROW){
                char *id = (char*)calloc(10, sizeof(char));
                char *product_name = (char*)calloc(50, sizeof(char));
                char *price = (char*)calloc(10, sizeof(char));
                strcpy(id, sqlite3_column_text(stmt, 0));
                strcpy(product_name, sqlite3_column_text(stmt, 1));
                strcpy(price, sqlite3_column_text(stmt, 2));
                strcat(result, "Id: ");
                strcat(result, id);
                strcat(result, " Numar produs: ");
                strcat(result, product_name);
                strcat(result, " Pret: ");
                strcat(result, price);
                strcat(result, "\n");
                free(id);
                free(product_name);
                free(price);
            }
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    return result;
}

char* creare_oferta(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    CommandParams params = parseCommand(cmd);
    if(params.numParams != 2){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "creare oferta: <nume_produs> <pret_produs>\n");
        freeCommandParams(&params);
        return result;
    }

    if(*id == -1){
        strcpy(result, "Nu sunteti logat!\n");
        freeCommandParams(&params);
        return result;
    }
    else{
        sqlite3_stmt *stmt_insert;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_insert;
        char* query_inset = sqlite3_mprintf("INSERT INTO Oferte (product_name, price, seller_id, buyer_id) VALUES ('%q', '%q', '%d', '%d');", params.params[0], params.params[1], *id, -1);
        res_insert = sqlite3_prepare_v2(db, query_inset, -1, &stmt_insert, 0);
        sqlite3_step(stmt_insert);
        sqlite3_finalize(stmt_insert);
        strcat(result, "Oferta a fost creata cu succes!\n");
        sqlite3_close(db);    
    }

    freeCommandParams(&params);
    return result;
}

char* modificare_oferta(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    CommandParams params = parseCommand(cmd);

    if(params.numParams != 3){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "modificare oferta: <id> <nume_produs> <pret_produs>\n");
        freeCommandParams(&params);
        return result;
    }

    if(*id == -1){
        strcpy(result, "Nu sunteti logat!\n");
        freeCommandParams(&params);
        return result;
    }
    else{
        sqlite3_stmt *stmt_upd, *stmt;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_upd, res_db;
        char *query1 = sqlite3_mprintf("SELECT * FROM Oferte WHERE id = '%q';", params.params[0]);
        res_db = sqlite3_prepare_v2(db, query1, -1, &stmt, 0);
        if(res_db != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
            exit(EXIT_FAILURE);
        }
        else{
            if(sqlite3_step(stmt) == SQLITE_ROW){
                int seller_id = sqlite3_column_int(stmt, 3);
                if(seller_id != *id){
                    strcat(result, "Nu puteti modifica oferta altui utilizator!\n");
                    freeCommandParams(&params);
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    return result;
                }
                int buyer_id = sqlite3_column_int(stmt, 4);
                if(buyer_id != -1){
                    strcat(result, "Nu puteti modifica o oferta care a fost deja cumparata!\n");
                    freeCommandParams(&params);
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    return result;
                }
            }
            else{
                strcat(result, "Nu exista oferta cu acest id!\n");
                freeCommandParams(&params);
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return result;
            }
        }
        sqlite3_finalize(stmt);

        char *query2 = sqlite3_mprintf("UPDATE Oferte SET product_name = '%q', price = '%q' WHERE id = '%q';", params.params[1], params.params[2], params.params[0]);
        res_upd = sqlite3_prepare_v2(db, query2, -1, &stmt_upd, 0);
        sqlite3_step(stmt_upd);
        sqlite3_finalize(stmt_upd);
        strcat(result, "Oferta a fost modificata cu succes!\n");
        sqlite3_close(db);
    }

    freeCommandParams(&params);
    return result;
}

char* stergere_oferta(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    CommandParams params = parseCommand(cmd);

    if(params.numParams != 1){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "stergere oferta: <id>\n");
        freeCommandParams(&params);
        return result;
    }

    if(*id == -1){
        strcpy(result, "Nu sunteti logat!\n");
        freeCommandParams(&params);
        return result;
    }
    else{
        sqlite3_stmt *stmt_upd, *stmt;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_upd, res_db;
        char *query1 = sqlite3_mprintf("SELECT * FROM Oferte WHERE id = '%q';", params.params[0]);
        res_db = sqlite3_prepare_v2(db, query1, -1, &stmt, 0);
        if(res_db != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
            exit(EXIT_FAILURE);
        }
        else{
            if(sqlite3_step(stmt) == SQLITE_ROW){
                int seller_id = sqlite3_column_int(stmt, 3);
                if(seller_id != *id){
                    strcat(result, "Nu puteti sterge oferta altui utilizator!\n");
                    freeCommandParams(&params);
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    return result;
                }
                int buyer_id = sqlite3_column_int(stmt, 4);
                if(buyer_id != -1){
                    strcat(result, "Nu puteti sterge o oferta care a fost deja cumparata!\n");
                    freeCommandParams(&params);
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    return result;
                }
            }
            else{
                strcat(result, "Nu exista oferta cu acest id!\n");
                freeCommandParams(&params);
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return result;
            }
        }
        sqlite3_finalize(stmt);

        char *query2 = sqlite3_mprintf("DELETE FROM Oferte WHERE id = '%q';", params.params[0]);
        res_upd = sqlite3_prepare_v2(db, query2, -1, &stmt_upd, 0);
        sqlite3_step(stmt_upd);
        sqlite3_finalize(stmt_upd);
        strcat(result, "Oferta a fost stearsa cu succes!\n");
        sqlite3_close(db);
    }

    freeCommandParams(&params);
    return result;
}

char* istoric_achizitii(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    if(*id == -1){
        strcpy(result, "Nu sunteti logat!\n");
        return result;
    }
    else{
        sqlite3_stmt *stmt;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_db;
        char *query1 = sqlite3_mprintf("SELECT * FROM Oferte WHERE buyer_id = '%d';", *id);
        res_db = sqlite3_prepare_v2(db, query1, -1, &stmt, 0);
        if(res_db != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
            exit(EXIT_FAILURE);
        }
        else{
            while(sqlite3_step(stmt) == SQLITE_ROW){
                char *id = (char*)calloc(10, sizeof(char));
                char *product_name = (char*)calloc(50, sizeof(char));
                char *price = (char*)calloc(10, sizeof(char));
                strcpy(id, sqlite3_column_text(stmt, 0));
                strcpy(product_name, sqlite3_column_text(stmt, 1));
                strcpy(price, sqlite3_column_text(stmt, 2));
                strcat(result, "Id: ");
                strcat(result, id);
                strcat(result, " Numar produs: ");
                strcat(result, product_name);
                strcat(result, " Pret: ");
                strcat(result, price);
                strcat(result, "\n");
                free(id);
                free(product_name);
                free(price);
            }
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    return result;
}

char* cumparare_produs(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    CommandParams params = parseCommand(cmd);

    if(params.numParams != 1){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "cumparare produs: <id>\n");
        freeCommandParams(&params);
        return result;
    }

    if(*id == -1){
        strcpy(result, "Nu sunteti logat!\n");
        freeCommandParams(&params);
        return result;
    }
    else{
        sqlite3_stmt *stmt_upd_prod, *stmt, *stmt2, *stmt3, *stmt_upd_seller, *stmt_upd_buyer;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_upd_prod, res_db, res_sold1, res_sold2, upd_sold1, upd_sold2;
        int pret, buyer_id, seller_id, sold_int1, sold_int2, new_sold1, new_sold2;

        char *query1 = sqlite3_mprintf("SELECT * FROM Oferte WHERE id = '%q';", params.params[0]);
        res_db = sqlite3_prepare_v2(db, query1, -1, &stmt, 0);
        if(res_db != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
            exit(EXIT_FAILURE);
        }
        else{
            if(sqlite3_step(stmt) == SQLITE_ROW){
                pret = sqlite3_column_int(stmt, 2);
                seller_id = sqlite3_column_int(stmt, 3);
                if(seller_id == *id){
                    strcat(result, "Nu puteti cumpara propria oferta!\n");
                    freeCommandParams(&params);
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    return result;
                }
                buyer_id = sqlite3_column_int(stmt, 4);
                if(buyer_id != -1){
                    strcat(result, "Oferta a fost deja cumparata!\n");
                    freeCommandParams(&params);
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    return result;
                }
            }
            else{
                strcat(result, "Nu exista oferta cu acest id!\n");
                freeCommandParams(&params);
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return result;
            }
        }
        sqlite3_finalize(stmt);

        char *query2 = sqlite3_mprintf("SELECT sold FROM Useri WHERE id = '%d';", *id);
        res_sold1 = sqlite3_prepare_v2(db, query2, -1, &stmt2, 0);
        if(res_sold1 != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
            exit(EXIT_FAILURE);
        }
        else{
            if(sqlite3_step(stmt2) == SQLITE_ROW){
                char *sold = (char*)calloc(10, sizeof(char));
                strcpy(sold, sqlite3_column_text(stmt2, 0));
                sold_int1 = atoi(sold);
                if(sold_int1 < pret){
                    strcat(result, "Nu aveti suficienti bani!\n");
                    free(sold);
                    freeCommandParams(&params);
                    sqlite3_finalize(stmt2);
                    sqlite3_close(db);
                    return result;
                }
                else{
                    new_sold1 = sold_int1 - pret;
                }
                free(sold);
            }
            else{
                strcat(result, "Eroare la obtinerea soldului dumneavoastra!\n");
                freeCommandParams(&params);
                sqlite3_finalize(stmt2);
                sqlite3_close(db);
                return result;
            }
        }
        sqlite3_finalize(stmt2);

        char *query3 = sqlite3_mprintf("SELECT sold FROM Useri WHERE id = '%d';", seller_id);
        res_sold2 = sqlite3_prepare_v2(db, query3, -1, &stmt3, 0);
        if(res_sold2 != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
            exit(EXIT_FAILURE);
        }
        else{
            if(sqlite3_step(stmt3) == SQLITE_ROW){
                char *sold = (char*)calloc(10, sizeof(char));
                strcpy(sold, sqlite3_column_text(stmt3, 0));
                sold_int2 = atoi(sold);
                new_sold2 = sold_int2 + pret;
                free(sold);
            }
            else{
                strcat(result, "Eroare la obtinerea soldului vanzatorului!\n");
                freeCommandParams(&params);
                sqlite3_finalize(stmt3);
                sqlite3_close(db);
                return result;
            }
        }
        sqlite3_finalize(stmt3);

        char *query_upd_prod = sqlite3_mprintf("UPDATE Oferte SET buyer_id = '%d' WHERE id = '%q';", *id, params.params[0]);
        res_upd_prod = sqlite3_prepare_v2(db, query_upd_prod, -1, &stmt_upd_prod, 0);
        sqlite3_step(stmt_upd_prod);
        sqlite3_finalize(stmt_upd_prod);

        char *query_upd_seller = sqlite3_mprintf("UPDATE Useri SET sold = '%d' WHERE id = '%d';", new_sold2, seller_id);
        upd_sold1 = sqlite3_prepare_v2(db, query_upd_seller, -1, &stmt_upd_seller, 0);
        sqlite3_step(stmt_upd_seller);
        sqlite3_finalize(stmt_upd_seller);

        char *query_upd_buyer = sqlite3_mprintf("UPDATE Useri SET sold = '%d' WHERE id = '%d';", new_sold1, *id);
        upd_sold2 = sqlite3_prepare_v2(db, query_upd_buyer, -1, &stmt_upd_buyer, 0);
        sqlite3_step(stmt_upd_buyer);
        sqlite3_finalize(stmt_upd_buyer);

        strcat(result, "Oferta a fost cumparata cu succes!\n");
        sqlite3_close(db);
    }

    freeCommandParams(&params);
    return result;
}

char* cautare_produs(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }
    
    CommandParams params = parseCommand(cmd);

    if(params.numParams != 1){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "cautare produs: <nume_produs>\n");
        freeCommandParams(&params);
        return result;
    }

    params.params[0] = strtok(params.params[0], "\n");

    if(*id == -1){
        strcpy(result, "Nu sunteti logat!\n");
        freeCommandParams(&params);
        return result;
    }
    else{
        sqlite3_stmt *stmt;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_db;
        char *query1 = sqlite3_mprintf("SELECT * FROM Oferte WHERE product_name LIKE '%%%q%%' AND buyer_id = -1;", params.params[0]);
        res_db = sqlite3_prepare_v2(db, query1, -1, &stmt, 0);
        if(res_db != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
            printf("Eroare la pregatirea interogarii: %s\n", sqlite3_errmsg(db));

            exit(EXIT_FAILURE);
        }
        else{
            while(sqlite3_step(stmt) == SQLITE_ROW){
                char *id = (char*)calloc(10, sizeof(char));
                char *product_name = (char*)calloc(50, sizeof(char));
                char *price = (char*)calloc(10, sizeof(char));
                strcpy(id, sqlite3_column_text(stmt, 0));
                strcpy(product_name, sqlite3_column_text(stmt, 1));
                strcpy(price, sqlite3_column_text(stmt, 2));
                strcat(result, "Id: ");
                strcat(result, id);
                strcat(result, " Numar produs: ");
                strcat(result, product_name);
                strcat(result, " Pret: ");
                strcat(result, price);
                strcat(result, "\n");
                free(id);
                free(product_name);
                free(price);
            }
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    freeCommandParams(&params);
    return result;
}

char* vizualizare_sold(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    if(*id == -1){
        strcpy(result, "Nu sunteti logat!\n");
        return result;
    }
    else{
        sqlite3_stmt *stmt;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_db;
        char *query1 = sqlite3_mprintf("SELECT sold FROM Useri WHERE id = '%d';", *id);
        res_db = sqlite3_prepare_v2(db, query1, -1, &stmt, 0);
        if(res_db != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
            exit(EXIT_FAILURE);
        }
        else{
            if(sqlite3_step(stmt) == SQLITE_ROW){
                char *sold = (char*)calloc(10, sizeof(char));
                strcpy(sold, sqlite3_column_text(stmt, 0));
                strcat(result, "Soldul dumneavoastra este: ");
                strcat(result, sold);
                strcat(result, "\n");
                free(sold);
            }
            else{
                strcat(result, "Eroare la vizualizarea soldului!\n");
            }
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    return result;
}

char* modificare_sold(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    CommandParams params = parseCommand(cmd);

    if(params.numParams != 2){
        strcat(result, "Numarul de parametrii este incorect!\n");
        strcpy(result, "Comanda trebuie sa fie de forma: \n");
        strcat(result, "modificare sold: <adaugare/retragere> <suma>\n");
        freeCommandParams(&params);
        return result;
    }

    if(*id == -1){
        strcpy(result, "Nu sunteti logat!\n");
        freeCommandParams(&params);
        return result;
    }
    else{
        sqlite3_stmt *stmt, *stmt_upd;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_db, res_upd;
        char *query1 = sqlite3_mprintf("SELECT sold FROM Useri WHERE id = '%d';", *id);
        res_db = sqlite3_prepare_v2(db, query1, -1, &stmt, 0);
        if(res_db != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
            exit(EXIT_FAILURE);
        }
        else{
            if(sqlite3_step(stmt) == SQLITE_ROW){
                char *sold = (char*)calloc(10, sizeof(char));
                strcpy(sold, sqlite3_column_text(stmt, 0));
                int sold_int = atoi(sold);
                int suma = atoi(params.params[1]);
                if(strcmp(params.params[0], "adaugare") == 0){
                    //printf("Am intrat in adaugare!\n");
                    if(suma < 0){
                        strcat(result, "Suma trebuie sa fie pozitiva!\n");
                        free(sold);
                        freeCommandParams(&params);
                        sqlite3_finalize(stmt);
                        sqlite3_close(db);
                        return result;
                    }
                    sold_int += suma;
                }
                else if(strcmp(params.params[0], "retragere") == 0){
                    //printf("Am intrat in retragere!\n");
                    if(sold_int < 0){
                        strcat(result, "Nu aveti suficienti bani!\n");
                        free(sold);
                        freeCommandParams(&params);
                        sqlite3_finalize(stmt);
                        sqlite3_close(db);
                        return result;
                    }
                    if(suma < 0){
                        strcat(result, "Suma trebuie sa fie pozitiva!\n");
                        free(sold);
                        freeCommandParams(&params);
                        sqlite3_finalize(stmt);
                        sqlite3_close(db);
                        return result;
                    }
                    sold_int -= suma;
                }
                else{
                    strcat(result, "Comanda invalida!\n");
                    free(sold);
                    freeCommandParams(&params);
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    return result;
                }
                char *query2 = sqlite3_mprintf("UPDATE Useri SET sold = '%d' WHERE id = '%d';", sold_int, *id);
                res_upd = sqlite3_prepare_v2(db, query2, -1, &stmt_upd, 0);
                sqlite3_step(stmt_upd);
                sqlite3_finalize(stmt_upd);
                strcat(result, "Soldul a fost modificat cu succes!\n");
                free(sold);
            }
            else{
                strcat(result, "Eroare la modificarea soldului!\n");
            }
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    freeCommandParams(&params);
    return result;
}

char* postari_proprii(char* cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if(result == NULL){
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }
    if(*id == -1){
        strcpy(result, "Nu sunteti logat!\n");
        return result;
    }
    else{
        sqlite3_stmt *stmt;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
        }
        int res_db;
        char *query1 = sqlite3_mprintf("SELECT * FROM Oferte WHERE seller_id = '%d';", *id);
        res_db = sqlite3_prepare_v2(db, query1, -1, &stmt, 0);
        if(res_db != SQLITE_OK){
            printf("Eroare la pregatirea interogarii!\n");
        }
        else{
            while(sqlite3_step(stmt) == SQLITE_ROW){
                char *id = (char*)calloc(10, sizeof(char));
                char *product_name = (char*)calloc(50, sizeof(char));
                char *price = (char*)calloc(10, sizeof(char));
                int buyer_id_int = sqlite3_column_int(stmt, 4);
                strcpy(id, sqlite3_column_text(stmt, 0));
                strcpy(product_name, sqlite3_column_text(stmt, 1));
                strcpy(price, sqlite3_column_text(stmt, 2));
                strcat(result, "Id: ");
                strcat(result, id);
                strcat(result, " Numar produs: ");
                strcat(result, product_name);
                strcat(result, " Pret: ");
                strcat(result, price);
                if(buyer_id_int != -1){
                    strcat(result, " Satatus: Vandut!\n");
                }
                else{
                    strcat(result, " Status: Disponibil!\n");
                }
                free(id);
                free(product_name);
                free(price);
            }
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    return result;
}

char* logout_client(char *cmd, int *id){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if(result == NULL){
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }
    if(*id == -1){
        strcpy(result, "Nu sunteti logat!\n");
        return result;
    }
    else{
        sqlite3_stmt *stmt_upd;
        sqlite3 *db;
        int open_db = sqlite3_open("mkDB.db", &db);
        if(open_db != SQLITE_OK){
            printf("Eroare la deschiderea bazei de date!\n");
            exit(EXIT_FAILURE);
        }
        int res_upd;

        char *query2 = sqlite3_mprintf("UPDATE Useri SET logged = 0 WHERE id = '%d';", *id);
        res_upd = sqlite3_prepare_v2(db, query2, -1, &stmt_upd, 0);
        sqlite3_step(stmt_upd);
        sqlite3_finalize(stmt_upd);
        strcat(result, "V-ati delogat cu succes!\n");
        sqlite3_close(db);
    }
    *id = -1;

    return result;
}

char* help_cmd(){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }
    
    strcpy(result, "Comenzile disponibile sunt:\n");
    strcat(result, "1) login: <username> <password>\n");
    strcat(result, "2) creare cont: <username> <password> <sold>\n");
    strcat(result, "3) lista produse\n");
    strcat(result, "4) creare oferta: <nume_produs> <pret_produs>\n");
    strcat(result, "5) modificare oferta: <id> <nume_produs> <pret_produs>\n");
    strcat(result, "6) stergere oferta: <id>\n");
    strcat(result, "7) istoric achizitii\n");
    strcat(result, "8) cumparare produs: <id>\n");
    strcat(result, "9) cautare produs: <nume_produs>\n");
    strcat(result, "10) vizualizare sold\n");
    strcat(result, "11) modificare sold: <adaugare/retragere> <suma>\n");
    strcat(result, "12) postari proprii\n");
    strcat(result, "13) logout\n");
    strcat(result, "14) exit\n");

    return result;
}

char* invalid_command(){
    char* result = (char*)calloc(MAX_COMMAND_LENGTH, sizeof(char));
    if (result == NULL) {
        perror("Eroare la alocarea de memorie");
        exit(EXIT_FAILURE);
    }

    strcpy(result, "Comanda este invalida!");

    return result;
}

char* manager_comenzi(char *comanda, int *id)
{
    if (strstr(comanda, "login"))
    {
        return login_client(comanda, id);
    }
    else if (strstr(comanda, "creare cont"))
    {
        return creare_cont(comanda, id);
    }
    else if (strstr(comanda, "lista produse"))
    {
        return lista_produse(comanda, id);
    }
    else if (strstr(comanda, "creare oferta"))
    {
        return creare_oferta(comanda, id);
    }
    else if (strstr(comanda, "modificare oferta"))
    {
        return modificare_oferta(comanda, id);
    }
    else if (strstr(comanda, "stergere oferta"))
    {
        return stergere_oferta(comanda, id);
    }
    else if (strstr(comanda, "istoric achizitii"))
    {
        return istoric_achizitii(comanda, id);
    }
    else if (strstr(comanda, "cumparare produs"))
    {
        return cumparare_produs(comanda, id);
    }
    else if (strstr(comanda, "cautare produs"))
    {
        return cautare_produs(comanda, id);
    }
    else if (strstr(comanda, "vizualizare sold"))
    {
        return vizualizare_sold(comanda, id);
    }
    else if (strstr(comanda, "modificare sold"))
    {
        return modificare_sold(comanda, id);
    }
    else if (strstr(comanda, "postari proprii"))
    {
        return postari_proprii(comanda, id);
    }
    else if (strstr(comanda, "help")){
        return help_cmd();
    }
    else if(strstr(comanda, "logout")){
        return logout_client(comanda, id);
    }
    else if (strstr(comanda, "exit"))
    {
        return exit_f(id);
    }
    else
    {
        return invalid_command();
    }
}

int main ()
{
  struct sockaddr_in server;	// structura folosita de server
  struct sockaddr_in from;	
  int nr;		//mesajul primit de trimis la client 
  int sd;		//descriptorul de socket 
  int pid;
  pthread_t th[100];    //Identificatorii thread-urilor care se vor crea
  int i=0;
  

  /* crearea unui socket */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server]Eroare la socket().\n");
      return errno;
    }
  /* utilizarea optiunii SO_REUSEADDR */
  int on=1;
  setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
  
  /* pregatirea structurilor de date */
  bzero (&server, sizeof (server));
  bzero (&from, sizeof (from));
  
  /* umplem structura folosita de server */
  /* stabilirea familiei de socket-uri */
    server.sin_family = AF_INET;	
  /* acceptam orice adresa */
    server.sin_addr.s_addr = htonl (INADDR_ANY);
  /* utilizam un port utilizator */
    server.sin_port = htons (PORT);
  
  /* atasam socketul */
  if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
      perror ("[server]Eroare la bind().\n");
      return errno;
    }

  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen (sd, 2) == -1)
    {
      perror ("[server]Eroare la listen().\n");
      return errno;
    }
  /* servim in mod concurent clientii...folosind thread-uri */
  while (1)
    {
      int client;
      thData * td; //parametru functia executata de thread     
      int length = sizeof (from);

      printf ("[server]Asteptam la portul %d...\n",PORT);
      fflush (stdout);

      // client= malloc(sizeof(int));
      /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
      if ( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0)
	{
	  perror ("[server]Eroare la accept().\n");
	  continue;
	}
	
        /* s-a realizat conexiunea, se astepta mesajul */
    
	// int idThread; //id-ul threadului
	// int cl; //descriptorul intors de accept

	td=(struct thData*)malloc(sizeof(struct thData));	
	td->idThread=i++;
	td->cl=client;

	pthread_create(&th[i], NULL, &treat, td);	      
				
	}//while    
};		

static void *treat(void * arg)
{		
		struct thData tdL; 
		tdL= *((struct thData*)arg);	
		printf ("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
		fflush (stdout);		 
		pthread_detach(pthread_self());		
		raspunde((struct thData*)arg);
		/* am terminat cu acest client, inchidem conexiunea */
		close ((intptr_t)arg);
		return(NULL);	
  		
};

void raspunde(void *arg)
{
    int id = -1;
    char nr[MAX_COMMAND_LENGTH];
    //int nr, 
    int i=0;
	struct thData tdL; 
	tdL= *((struct thData*)arg);
    while (1)
    {
        fflush(stdout);
        bzero(nr, sizeof(nr));
        int bytes_read;
        if ((bytes_read = read (tdL.cl, &nr, sizeof(nr))) <= 0)
        {
          if(bytes_read == 0){
            printf("[Thread %d] Clientul s-a deconectat.\n", tdL.idThread);
          }
          else{
            printf("[Thread %d]\n",tdL.idThread);
            perror ("Eroare la read() de la client.\n");    
          }
          break;
        }
        
        printf ("[Thread %d]Mesajul a fost receptionat...%s\n",tdL.idThread, nr);
                char *m;
                //memset(m,0,50);
                m = manager_comenzi(nr, &id);
                /*pregatim mesajul de raspuns */
                //nr++;      
        printf("[Thread %d]Trimitem mesajul inapoi...%s\n",tdL.idThread, m);
                
                
                /* returnam mesajul clientului */
        if (write (tdL.cl, m, strlen(m + 1) +1) <= 0)
        {
            printf("[Thread %d] ",tdL.idThread);
            perror ("[Thread]Eroare la write() catre client.\n");
        }
        free(m);
        fflush(stdout);
        
    }
}
