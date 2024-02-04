#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>
#include <getopt.h>

#define BUFFER_SIZE 4096


bool types_only_flag = false;
bool copy_search_string = false;
bool matched = false;

// typ zpravy
bool bindrequest_flag = false;
bool searchrequest_flag = false;
bool unbindrequest_flag = false;

bool error_flag = false;
uint8_t error_type = 0x50;

unsigned long long sizelimit = 1;

unsigned long long idx = 0; // index odpovedi
size_t dynamic_size = 10; // velikost odpovedi
int match_cnt = 0;  // pocet matchnutych radku

uint8_t dn_string[BUFFER_SIZE]; // obshahuje hodnotu dn

int isBitSet(uint8_t byte, int bitIndex) // zjisti jestli je dany bit nastaveny na 1
{
    return (byte >> bitIndex) & 0x01;
}

unsigned long long  lenght_decode(uint8_t received_message[], int *index, uint8_t search_string[], unsigned long long *search_string_index) // dekoduje hondotu delky
{
    unsigned long long  length = 0, length_of_length = 0;
    int exp = 0;
    uint8_t current_byte = received_message[*index];
    if(!isBitSet(received_message[*index],7)) // jednoducha delka
    {
        length = current_byte & 0x7F;
        if(copy_search_string)
        {
            search_string[*search_string_index] = received_message[*index];
            *search_string_index = *search_string_index + 1;
        }
    }
    else // slozena delka s delkou delky
    {
        length_of_length = current_byte & 0x7F;
        if(length_of_length != 0)
        {
            for(int i = *index+length_of_length; i > *index; i--) // secteni bitu
            {
                current_byte = received_message[i];
                if(copy_search_string)
                {
                    search_string[*search_string_index] = received_message[*index];
                    *search_string_index = *search_string_index + 1;
                }
                for(int j = 0; j < 8; j++) 
                {  
                    length += isBitSet(current_byte, j) * (unsigned long long)pow(2, exp);
                    exp++;
                }
            }
        }
        else
        {
            perror("Wrong length syntax");
            error_flag = true;
            error_type = 0x02;
            return 0;
        }
    }
    *index = *index + length_of_length;
    *index = *index + 1; // index dekodovane zpravy
    return length;
}


void octet_decode_with_length(uint8_t received_message[], int *index, unsigned long long length, uint8_t decoded_string[]) // dekodovani octet stringu (0x04)
{
        for (unsigned long long i = 0; i < length; i++) // prekopirovani odpovidajicich prvku do navratoveho pole
        {
            decoded_string[i] = received_message[*index + i];
        }
        *index = *index + length; // index dekodovane zpravy
}



bool is_matched(char line[], bool filter, uint8_t search_string[], unsigned long long search_string_size, long long unsigned end) // kontrola shody prijatych atributu s atributy radku
{
    bool match = filter, not_flag = false;
    long long unsigned length , i = 0, type;
    char search_text[search_string_size];
    char copy_line[1024];
    char *token;
    if(filter) // and filter
    {
        match = true;
    }
    else // or filter
    {
        match = false;
    }

    while(idx < search_string_size && idx < end) // prohledani vyhledavajiciho retezce
    {
        switch (search_string[idx]) // zvoleni filtru
            {
                case 0xa3: // EQUAL
                    idx++;
                    length = search_string[idx]; 
                    idx++;
                    if(search_string[idx] == 0x04)
                    {
                        idx++;
                    }
                    else
                    {
                        perror("Wrong type\n");
                        return false;
                    }
                    switch (search_string[idx])
                    {
                    case 0x02: // cn + kontrola jestli je to cn
                        type = 0;
                        idx++;
                        if(search_string[idx] != 'c')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        if(search_string[idx] != 'n')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        //idx = idx + 3;
                        break;
                    
                    case 0x03: // uid + kontrola jestli je to uid
                        type = 1;
                        idx++;
                        if(search_string[idx] != 'u')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        if(search_string[idx] != 'i')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        if(search_string[idx] != 'd')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        //idx = idx + 4;
                        break;

                    case 0x04: // mail + kontrola jestli je to mail
                        type = 2;
                        idx++;
                        if(search_string[idx] != 'm')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        if(search_string[idx] != 'a')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        if(search_string[idx] != 'i')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        if(search_string[idx] != 'l')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        //idx = idx + 5; 
                        break;

                    default:
                        perror("ERROR not supported atribute");
                        return false;
                        break;
                    }
                    if(search_string[idx] == 0x04) // nasleduje pouze octet string
                    {
                        idx++;
                    }
                    else
                    {
                        perror("ERROR not suported type");
                        return false;
                    }
                    length = search_string[idx];
                    idx++;
                    i = idx;
                    while(i < idx + length) // naplneni retezce k porovnani
                    {
                        search_text[i-idx] = search_string[i];
                        i++;
                    }
                    search_text[i-idx] = '\0'; // ukonceni retezce
                    idx = i;
                    i = 0;
                    strcpy(copy_line, line);
                    token = strtok(copy_line, ";"); // rozdeleni radku oddelovacem ";"
                    while (token != NULL && i < type) // rozdeleni radku oddelovacem ";" podle typu (cn=0, uid=1, mail=2)
                    {
                        i++;
                        if(i <= type)
                        {
                            token = strtok(NULL, ";");
                        }
                    }
                    
                    
                    if(filter) // and 
                    {
                        if(!not_flag) 
                        {
                            // porovnani hledaneho atributu a atributu radku 
                            if(!(strncmp(search_text, token, strlen(search_text)) == 0 && ((strlen(search_text) == strlen(token)) || (strlen(search_text) == strlen(token) - 1) || (strlen(search_text) == strlen(token) - 2))))
                            {
                                match = false; // and logika => staci aby jeden byl false => cely vyraz je false
                            }
                        }
                        else // not filter
                        {
                            // porovnani hledaneho atributu a atributu radku akorat podminka je znegovana oproti predchozi
                            if(strncmp(search_text, token, strlen(search_text)) == 0 && ((strlen(search_text) == strlen(token)) || (strlen(search_text) == strlen(token) - 1) || (strlen(search_text) == strlen(token) - 2)))
                            {
                                match = false; // and logika => staci aby jeden byl false => cely vyraz je false
                            }
                        }
                    }
                    else // or
                    {
                        if(!not_flag)
                        {
                            // porovnani hledaneho atributu a atributu radku
                            if((strncmp(search_text, token, strlen(search_text)) == 0) && ((strlen(search_text) == strlen(token)) || (strlen(search_text) == strlen(token) - 1) || (strlen(search_text) == strlen(token) - 2)))
                            {
                                match = true; // or logika => staci aby jeden byl true => cely vyraz je true
                            }
                        }
                        else // not filter
                        {
                            // porovnani hledaneho atributu a atributu radku akorat podminka je znegovana oproti predchozi
                            if(!(strncmp(search_text, token, strlen(search_text)) == 0 && ((strlen(search_text) == strlen(token)) || (strlen(search_text) == strlen(token) - 1) || (strlen(search_text) == strlen(token) - 2))))
                            {
                                match = true; // or logika => staci aby jeden byl true => cely vyraz je true
                            }
                        }
                    }
                    not_flag = false; // not flag zahrnuje jen jeden atribut
                    break;
                
                case 0xa4: // SUBSTRING
                    idx++;
                    length = search_string[idx];
                    idx++;
                    if(search_string[idx] == 0x04)
                    {
                        idx++;
                    }
                    else
                    {
                        perror("Wrong type");
                        return false;
                    }
                    switch (search_string[idx])
                    {
                    case 0x02:  // cn + kontrola jestli je to cn
                        type = 0;
                        idx++;
                        if(search_string[idx] != 'c')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        if(search_string[idx] != 'n')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        //idx = idx + 3;
                        break;
                    
                    case 0x03:  // uid + kontrola jestli je to uid
                        type = 1;
                        idx++;
                        if(search_string[idx] != 'u')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        if(search_string[idx] != 'i')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        if(search_string[idx] != 'd')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        //idx = idx + 4;
                        break;

                    case 0x04:  // mail + kontrola jestli je to mail
                        type = 2;
                        idx++;
                        if(search_string[idx] != 'm')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        if(search_string[idx] != 'a')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        if(search_string[idx] != 'i')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        if(search_string[idx] != 'l')
                        {
                            error_flag = true;
                            return false;
                        }
                        idx++;
                        //idx = idx + 5; // 6
                        break;

                    default:
                        perror("ERROR not supported atribute");
                        return false;
                        break;
                    }
                    if(search_string[idx] == 0x04) // nasleduje pouze octet string
                    {
                        idx++;
                    }
                    else
                    {
                        perror("ERROR not suported type");
                        return false;
                    }
                    length = search_string[idx];
                    idx++;
                    i = idx;
                    while(i < idx + length) // naplneni retezce k porovnani
                    {
                        search_text[i-idx] = search_string[i];
                        i++;
                    }
                    search_text[i-idx] = '\0'; // ukonceni retezce
                    idx = i;
                    i = 0;
                    strcpy(copy_line, line);
                    token = strtok(copy_line, ";");  // rozdeleni radku oddelovacem ";"
                    while (token != NULL && i < type) // rozdeleni radku oddelovacem ";" podle typu (cn=0, uid=1, mail=2)
                    {
                        i++;
                        if(i <= type)
                        {
                            token = strtok(NULL, ";"); 
                        }
                    }
                    
                    
                    if(filter) // and 
                    {
                        if(!not_flag)
                        {
                            // porovnani hledaneho atributu
                            if(!(strstr(token, search_text) != NULL))
                            {
                                match = false; // and logika => staci aby jeden byl false => cely vyraz je false
                            }
                        }
                        else // not filter
                        {
                            // porovnani hledaneho atributu a atributu radku akorat podminka je znegovana oproti predchozi
                            if(strstr(token, search_text) != NULL)
                            {
                                match = false; // and logika => staci aby jeden byl false => cely vyraz je false
                            }
                        }
                    }
                    else // or
                    {
                        if(!not_flag)
                        {
                            // porovnani hledaneho atributu
                            if(strstr(token, search_text) != NULL)
                            {
                                match = true; // or logika => staci aby jeden byl true => cely vyraz je true
                            }
                        }
                        else // not filter
                        {
                            // porovnani hledaneho atributu a atributu radku akorat podminka je znegovana oproti predchozi
                            if(!(strstr(token, search_text) != NULL))
                            {
                                match = true; // or logika => staci aby jeden byl true => cely vyraz je true
                            }
                        }
                    }
                    not_flag = false;
                    break;

                case 0xa0: // and
                    idx = idx + 2;
                    match = is_matched(line, true, search_string, search_string_size, search_string[idx-1]+idx-1); // funkce se rekurzivne zavola s odpovidajicim filtrem
                    break; 
                    
                case 0xa1: // or
                    idx = idx + 2;
                    match = is_matched(line, false, search_string, search_string_size, search_string[idx-1]+idx-1); // funkce se rekurzivne zavola s odpovidajicim filtrem
                    break; 
                
                case 0xa2: // not
                    idx = idx + 2; // preskoceni znaku 0xa2 a delky
                    not_flag = true; 
                    break; 
                default:
                    idx++;
                    break;
            }
    }
    return match;
}






void file_search(uint8_t searched_string[], int search_string_size, int client_socket, char *file_path)
{
    size_t token_length, i;
    dynamic_size = 10;
    int response_index, sequence_length_index;
    char *token;

    bool match;

    uint8_t *response_message =  malloc(sizeof(uint8_t));
    
    if (response_message == NULL) 
    {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    FILE *file = fopen(file_path, "r"); 
    if(file == NULL) 
    {
        perror("Error opening the file");
        error_flag = true;
        error_type = 0x35;
        return;
    }
    char line[1024];
    char copy_line[1024];
    while(fgets(line, sizeof(line), file) != NULL && sizelimit > 0 && !error_flag) // prohledani souboru
    {
        dynamic_size = 11 + strlen((const char *)dn_string); // search response entry zprava clientovi
        response_message = (uint8_t *)realloc(response_message, dynamic_size * sizeof(uint8_t));
        response_message[0] = 0x30;
        response_message[1] = 0x00; // change
        response_message[2] = 0x02;
        response_message[3] = 0x01;
        response_message[4] = 0x02;
        response_message[5] = 0x64;
        response_message[6] = 0x00; // change
        response_message[7] = 0x04;
        response_message[8] = strlen((const char *)dn_string);
        response_index = 9;
        for(i = 0; i < strlen((const char *)dn_string); i++) // zprava obsahuje dn
        {
            response_message[response_index + i] = dn_string[i];
        }
        response_index = response_index + strlen((const char *)dn_string); 
        response_message[response_index] = 0x30;
        response_index++;
        response_message[response_index] = 0x00; // change
        sequence_length_index = response_index;
        response_index++;
        
        idx = 0;
        if(strlen(line) > 2) // radek neni "\0\n" apod...
        {
            match = is_matched(line, false, searched_string, search_string_size, search_string_size-1); // kontrola shody
            if(error_flag) 
            {
                perror("Wrong atribute type");
                free(response_message);
                fclose(file);
                return;
            }
        }
        if(match)
        {
            matched = true;
            sizelimit--; // nalezen vysledek zbyva limit sizelimit-1
            match_cnt++; // pocet nalezenych shod
        }

        i = 0;
        strcpy(copy_line, line);
        token = strtok(copy_line, ";"); // rozdeleni radku oddelovacem ";"
        while (token != NULL && match) // rozdeleni radku oddelovacem ";" podle typu (cn=0, uid=1, mail=2)
        {
            token_length = strlen(token);
            if(i==0) // cn
            {
                if(!types_only_flag) // i s hodnotou
                {
                    dynamic_size = dynamic_size + token_length + 10; // zvetseni velikosti odpovedi o nasledujici prvky
                    response_message = (uint8_t *)realloc(response_message, dynamic_size * sizeof(uint8_t));
                    response_message[response_index] = 0x30;
                    response_index++;
                    response_message[response_index] = 8 + token_length;
                    response_index++;

                    response_message[response_index] = 0x04;
                    response_index++;
                    response_message[response_index] = 0x02;
                    response_index++;
                    response_message[response_index] = 0x63; // c
                    response_index++;
                    response_message[response_index] = 0x6E; // n
                    response_index++;

                    response_message[response_index] = 0x31; 
                    response_index++;
                    response_message[response_index] = 2 + token_length;
                    response_index++;

                    response_message[response_index] = 0x04;
                    response_index++;
                    response_message[response_index] = token_length;
                    response_index++;
                    for (unsigned long long i = 0; i < token_length; i++) // matchnuta hodnota cn
                    {
                        response_message[response_index + i] = token[i];
                    }
                    response_index = response_index + token_length;
                }
               else // hodnota se neposila => je nulova
                {
                    dynamic_size = dynamic_size + 8; // zvetseni velikosti odpovedi o nasledujici prvky
                    response_message = (uint8_t *)realloc(response_message, dynamic_size * sizeof(uint8_t));
                    response_message[response_index] = 0x30;
                    response_index++;
                    response_message[response_index] = 6;
                    response_index++;

                    response_message[response_index] = 0x04;
                    response_index++;
                    response_message[response_index] = 0x02;
                    response_index++;
                    response_message[response_index] = 0x63; // c
                    response_index++;
                    response_message[response_index] = 0x6E; // n
                    response_index++;

                    response_message[response_index] = 0x31; 
                    response_index++;
                    response_message[response_index] = 0x00; // nulova hodnota
                    response_index++;

                }
            }
            if(i==2) // mail
            {

                token_length = token_length - 2; //  '/0' '/n' kvuli konci radku

                if(!types_only_flag) // i s hodnotou
                {
                    dynamic_size = dynamic_size + 12 + token_length; // zvetseni velikosti odpovedi o nasledujici prvky
                    response_message = (uint8_t *)realloc(response_message, dynamic_size * sizeof(uint8_t));
                    response_message[response_index] = 0x30;
                    response_index++;
                    response_message[response_index] = 10 + token_length; 
                    response_index++;

                    response_message[response_index] = 0x04;
                    response_index++;
                    response_message[response_index] = 0x04;
                    response_index++;
                    response_message[response_index] = 0x6D; // m
                    response_index++;
                    response_message[response_index] = 0x61; // a
                    response_index++;
                    response_message[response_index] = 0x69; // i
                    response_index++;
                    response_message[response_index] = 0x6C; // l
                    response_index++;

                    response_message[response_index] = 0x31;
                    response_index++;
                    response_message[response_index] = 2 + token_length;
                    response_index++;

                    response_message[response_index] = 0x04; // string
                    response_index++;
                    response_message[response_index] = token_length; 
                    response_index++;
                    for (unsigned long long i = 0; i < token_length; i++) // matchnuta hodnota mail
                    {
                        response_message[response_index + i] = token[i];
                    }
                    response_index = response_index + token_length;
                }
                else // hodnota se neposila => je nulova
                {
                    dynamic_size = dynamic_size + 10; // zvetseni velikosti odpovedi o nasledujici prvky
                    response_message = (uint8_t *)realloc(response_message, dynamic_size * sizeof(uint8_t));
                    response_message[response_index] = 0x30;
                    response_index++;
                    response_message[response_index] = 8;
                    response_index++;

                    response_message[response_index] = 0x04;
                    response_index++;
                    response_message[response_index] = 0x04;
                    response_index++;
                    response_message[response_index] = 0x6D; // m
                    response_index++;
                    response_message[response_index] = 0x61; // a
                    response_index++;
                    response_message[response_index] = 0x69; // i
                    response_index++;
                    response_message[response_index] = 0x6C; // l
                    response_index++;

                    response_message[response_index] = 0x31; 
                    response_index++;
                    response_message[response_index] = 0x00; // nulova hodnota
                    response_index++;
                }
            }

            i++;
            token = strtok(NULL, ";");
        }

        if(match && strlen(line) > 1) // match radku "\n" se nepocita
        {
            // nastaveni delek
            response_message[1] = dynamic_size - 2;
            response_message[6] = dynamic_size - 7;
            response_message[sequence_length_index] = dynamic_size - 11 - strlen((const char *)dn_string);

            if (send(client_socket, response_message, dynamic_size, 0) == -1) // odeslani search response entry
            {
                perror("ERROR when sending message");
                close(client_socket);
                exit(EXIT_FAILURE);
            }

        }
    }

    fclose(file);


    dynamic_size = 10; // nastaveni velikosti odpovedi
    response_message = (uint8_t *)realloc(response_message, dynamic_size * sizeof(uint8_t));
    response_message[0] = 0x30;
    response_message[1] = 0x00; // change
    response_message[2] = 0x02;
    response_message[3] = 0x01;
    response_message[4] = 0x02;
    response_message[5] = 0x65;
    response_message[6] = 0x00; // change
    response_message[7] = 0x0a;
    response_message[8] = 0x01;
    response_message[9] = 0x00;

    // chybova hlaska
    if(sizelimit <= 0)
    {
        response_message[9] = 0x04;
    }

    if(error_flag)
    {
        response_message[9] = 0x01;
    }

    if(matched) // alespon jeden match
    {
        // vytvoreni zpravy search done response
        dynamic_size = dynamic_size + 4 + strlen((const char *)dn_string);
        response_message = (uint8_t *)realloc(response_message, dynamic_size * sizeof(uint8_t));
        response_message[1] = dynamic_size - 2;
        response_message[6] = dynamic_size - 7;
        response_message[10] = 0x04;
        response_message[11] = strlen((const char *)dn_string); 
        response_index = 12;
        for(i = 0; i < strlen((const char *)dn_string); i++) 
        {
            response_message[response_index + i] = dn_string[i]; // matchnuty dn
        }
        response_index = response_index + strlen((const char *)dn_string);
        response_message[response_index] = 0x04;
        response_index++;
        response_message[response_index] = 0x00;
        response_index++;
    }
    else // zadny match
    {
        // vytvoreni zpravy search done response
        dynamic_size = dynamic_size + 4;
        response_message = (uint8_t *)realloc(response_message, dynamic_size * sizeof(uint8_t));
        response_message[1] = dynamic_size - 2;
        response_message[6] = dynamic_size - 7;
        response_message[10] = 0x04;
        response_message[11] = 0x00;
        response_message[12] = 0x04;
        response_message[13] = 0x00;
    }

    if (send(client_socket, response_message, dynamic_size, 0) == -1) // odeslani search done response
    {
        perror("ERROR when sending message");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    free(response_message);
}


long long int int_decode(uint8_t received_message[], int *index, unsigned long long length) // dekodovani hodnoty int
{
    long long int value = 0; 
    int exp = 0;
    uint8_t current_byte = received_message[*index];
    for(int i = *index + length - 1; i >= *index; i--) // pruchod byty
    {
        current_byte = received_message[i];
        for(int j = 0; j < 8; j++) // secteni bitu
        {  
            value += isBitSet(current_byte, j) * (long long int)pow(2, exp);
            exp++;
        }
    }
    *index = *index + length;
    return value;
}


int decode(uint8_t received_message[], ssize_t bytes_received, uint8_t search_string[]) // dekodovani zpravy
{
    
    int index = 0, filters_cnt = 0; // filters_cnt: pomocna hodnota pri dekodovani filtru
    long long int int_value;
    unsigned long long i = 0, search_string_index = 0;
    unsigned long long length;
    
    

    while(index < bytes_received) 
    {
        switch (received_message[index])
        {
        case 0x01: // bool
            index++;
            index++;
            if(received_message[index] == 0x00)
            {
                if(filters_cnt == 8)
                {
                    types_only_flag = false;
                }
            }
            else
            {
                if(filters_cnt == 8)
                {
                    types_only_flag = true;
                }
            }
            index++;
            filters_cnt++;
            break;

        case 0x02: // int
            index++;
            length = lenght_decode(received_message, &index, search_string, &search_string_index); // delka
            if(error_flag)
            {
                return -1;
            }
            int_value = int_decode(received_message, &index, length); // honota
            switch (filters_cnt)
            {
            case 1: // message id
                break;
            
            case 6: // size limit
                sizelimit = int_value;
                if(sizelimit <= 0)
                {
                    sizelimit = 1;
                }
                break; 
            

            default:
                break;
            }
            filters_cnt++;
            break;

        case 0x04: // octet string
            index++;
            if(copy_search_string) // nacitani vyhledavaciho filtru
            {
                search_string[search_string_index] = 0x04;
                search_string_index++;
            }
            length = lenght_decode(received_message, &index, search_string, &search_string_index); // delka
            if(error_flag)
            {
                return -1;
            }
            if(length == 0 && searchrequest_flag && filters_cnt == 3) // autentizace clienta neni podporovana
            {
                perror("Simple authentication is unsupported");
                error_flag = true;
                error_type = 0x07;
                return -1;
            }
            else
            {
                uint8_t decoded_string[length];
                octet_decode_with_length(received_message, &index, length, decoded_string); // hodnota
                if(copy_search_string) // nacitani vyhledavaciho filtru
                {
                    for(i=0; i<sizeof(decoded_string); i++)
                    {      
                        search_string[search_string_index] = decoded_string[i];
                        search_string_index = search_string_index + 1;
                    }
                }
                if(filters_cnt == 3) // retezec dn 
                {
                    decoded_string[sizeof(decoded_string)] = '\0';
                    strcpy((char *)dn_string, (const char *)decoded_string);
                }
            }


            if(received_message[index] == 0x30 && received_message[index+2] == 0x82) // substring filtr
            {
                index = index + 3; // preskoceni popisujicich prvku => nejsou potreba pri vyhledavani ve funcki is_matched
                search_string[search_string_index] = 0x04;
                search_string_index++;
                length = lenght_decode(received_message, &index, search_string, &search_string_index); // delka
                uint8_t decoded_string[length];
                octet_decode_with_length(received_message, &index, length, decoded_string); // hodnota
                if(copy_search_string) // nacitani vyhledavaciho filtru
                {
                    for(i=0; i<sizeof(decoded_string); i++)
                    {      
                        search_string[search_string_index] = decoded_string[i];
                        search_string_index = search_string_index + 1;
                    }
                }

            }

            if(received_message[index] == 0x30) // konec vyhledavaciho filtru
            {
                copy_search_string = false;
            }


            filters_cnt++;
            break;

        case 0x0a: // vlastnosti zpravy
            index = index + 2;
            int_value = int_decode(received_message, &index, 1);
            if(filters_cnt == 4)
            {
                if(int_value != 0) // podporuje se jen base scope(0)
                {
                    perror("Unsupported search scope");
                    error_flag = true;
                    return -1;
                }
            }
            filters_cnt++;
            break;

        case 0x30: // sekvence
            index++;
            length = lenght_decode(received_message, &index, search_string, &search_string_index); // delka
            if(error_flag)
            {
                return -1;
            }
            filters_cnt++;
            break;

        case 0x42: // unbind request
            unbindrequest_flag = true;
            index++;
            length = lenght_decode(received_message, &index, search_string, &search_string_index); // delka
            if(error_flag)
            {
                return -1;
            }
            filters_cnt++;
            break;

        case 0x60: // ldap bind 
            bindrequest_flag = true;
            index++;
            length = lenght_decode(received_message, &index, search_string, &search_string_index); // delka
            if(error_flag)
            {
                return -1;
            }
            filters_cnt++;
            break;
        
        case 0x63: // ldap search
            searchrequest_flag = true;
            index++;
            length = lenght_decode(received_message, &index, search_string, &search_string_index); // delka
            if(error_flag)
            {
                return -1;
            }
            filters_cnt++;
            break;   
        
        case 0xa0: // and 
                index++;
                if(copy_search_string) // nacitani vyhledavaciho filtru
                {
                    search_string[search_string_index] = 0xa0;
                    search_string_index++;
                }
                length = lenght_decode(received_message, &index, search_string, &search_string_index); // delka
                if(error_flag)
                {
                    return -1;
                }
                filters_cnt++;
            break;

        case 0xa1: // or
                index++;
                if(copy_search_string) // nacitani vyhledavaciho filtru
                {
                    search_string[search_string_index] = 0xa1;
                    search_string_index++;
                }
                length = lenght_decode(received_message, &index, search_string, &search_string_index); // delka
                if(error_flag)
                {
                    return -1;
                }
                filters_cnt++;
            break;

        case 0xa2: // not 
                index++;
                if(copy_search_string) // nacitani vyhledavaciho filtru
                {
                    search_string[search_string_index] = 0xa2;
                    search_string_index++;
                }
                length = lenght_decode(received_message, &index, search_string, &search_string_index); // delka
                if(error_flag)
                {
                    return -1;
                }
                filters_cnt++;
            break;

        case 0xa3: // equal
                index++;
                if(copy_search_string) // nacitani vyhledavaciho filtru
                {
                    search_string[search_string_index] = 0xa3;
                    search_string_index++;
                }
                length = lenght_decode(received_message, &index, search_string, &search_string_index); // delka
                if(error_flag)
                {
                    return -1;
                }
                filters_cnt++;
            break;

        case 0xa4: // substring
                index++;
                if(copy_search_string) // nacitani vyhledavaciho filtru
                {
                    search_string[search_string_index] = 0xa4;
                    search_string_index++;
                }
                length = lenght_decode(received_message, &index, search_string, &search_string_index); // delka
                search_string[search_string_index - 1] = search_string[search_string_index - 1] - 2;
                if(error_flag)
                {
                    return -1;
                }
                filters_cnt++;
            break;    

        default:
            index++;
            break;
        }

        if(filters_cnt == 8) // zacatek vyhledavaciho filtru ve zprave
        {
            copy_search_string = true;
        }
        
    }

    return search_string_index;
}




void send_err_msg(int client_socket) // search request done s chybovou hlaskou
{
    uint8_t error_response[14];
    error_response[0] = 0x30;
    error_response[1] = 0x0c; 
    error_response[2] = 0x02;
    error_response[3] = 0x01;
    error_response[4] = 0x02;
    error_response[5] = 0x65;
    error_response[6] = 0x07; 
    error_response[7] = 0x0a;
    error_response[8] = 0x01;
    error_response[9] = error_type; // typ chyby
    error_response[10] = 0x04;
    error_response[11] = 0x00;
    error_response[12] = 0x04;
    error_response[13] = 0x00;

    if (send(client_socket, error_response, 14, 0) == -1) 
    {
        perror("Chyba při odesílání zprávy");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    close(client_socket);
    exit(EXIT_SUCCESS);
}



int main(int argc, char *argv[]) 
{
    
    int PORT = 389;
    char *file_path = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "p:f:")) != -1) // zpracovani argumentu
    {
        switch (opt) 
        {
            case 'p':
                PORT = atoi(optarg);
                break;
            case 'f':
                file_path = optarg;
                break;
            default:
                printf("Usage: %s [-p <port>] -f <file_path>\n", argv[0]);
                perror("Wrong arguments");
                exit(EXIT_FAILURE);
        }
    }

    if (file_path == NULL) 
    {
        printf("File path is compulsory. Usage: %s [-p <port>] -f <file_path>\n", argv[0]);
        perror("Missing arguments");
        exit(EXIT_FAILURE);
    }


    // vytvoreni socketu
    int server_socket;
    server_socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == -1) 
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }


    // nastaveni socketu na podporu IPv4 i IPv6
    int option = 0; 
    if( setsockopt(server_socket, IPPROTO_IPV6, IPV6_V6ONLY, &option, sizeof(option)) )
    {
        perror("setsockopt() failed");
        exit(EXIT_FAILURE);
    }




    struct sockaddr_in6 server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin6_family = AF_INET6;
    server_address.sin6_port = htons(PORT);
    server_address.sin6_addr = in6addr_any; 

    // nabindovani socketu na IPv4 a IPv6
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) 
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // naslouchani
    if (listen(server_socket, 1) == -1) 
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    int client_socket;
    struct sockaddr_in6 client_address;
    socklen_t client_address_length = sizeof(client_address);


    char client_ip[INET6_ADDRSTRLEN];

    printf("Server listening on %s:%d\n", 
           inet_ntop(AF_INET6, &server_address.sin6_addr, client_ip, INET6_ADDRSTRLEN), 
           ntohs(server_address.sin6_port));


    // zprava o uspesnem nabindovani
    uint8_t bind_response[] = {0x30, 0x0C, 0x02, 0x01, 0x01, 0x61, 0x07, 0x0A, 0x01, 0x00, 0x04, 0x00, 0x04, 0x00};
    size_t bind_response_length = sizeof(bind_response);



    uint8_t search_string[BUFFER_SIZE]; // vyhledavaci filter 
  
    int search_string_size = 0;

    bool connect = false;

    pid_t id = 1;

    while (1) // vyrizovani pozadavku
    {
        if(!connect && id != 0)
        {
            client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
            if (client_socket == -1) 
            {
                perror("Accept failed");
                continue;
            }
            else
            {
                id = fork(); // rozdeleni procesu, matersky pokracuje v naslouchani, druhy obsluhuje clienta
                if(id == 0)
                {
                    connect = true;
                }
            }
        }    

        
        if(connect && id == 0) // obsluha clienta
        {
            uint8_t received_message[BUFFER_SIZE]; 
            ssize_t bytes_received = recv(client_socket, received_message, sizeof(received_message), 0);
            if (bytes_received == -1) 
            {
                perror("Chyba při přijímání zprávy od klienta");
                close(client_socket);
                exit(EXIT_FAILURE);
            }
            else if (bytes_received > 0)
            {
                search_string_size = decode(received_message, bytes_received, search_string); // dekodovani
                if(error_flag)
                {
                    send_err_msg(client_socket); // chyba, zaslani search done response s chybovou hlaskou
                }
                if(bindrequest_flag)
                {
                    if (send(client_socket, bind_response, bind_response_length, 0) == -1) 
                    {
                        perror("ERROR when sending message");
                        close(client_socket);
                        exit(EXIT_FAILURE);
                    }
                }
                if(searchrequest_flag)
                {
                    file_search(search_string, search_string_size, client_socket, file_path); // prohledani souboru
                    if(error_flag)
                    {
                        send_err_msg(client_socket); // chyba, zaslani search done response s chybovou hlaskou
                    }
                }
                if(unbindrequest_flag)
                {
                    close(client_socket);
                    connect = false;
                    exit(EXIT_SUCCESS);
                }
                bindrequest_flag = false;
                searchrequest_flag = false;
                unbindrequest_flag = false;            
            }
        }
    }
}

