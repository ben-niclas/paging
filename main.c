#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/random.h>

uint8_t hd_mem[4194304];
uint8_t ra_mem[655356];
uint8_t cross_value = -1;
struct seitentabellen_zeile {
	uint8_t present_bit;
	uint8_t dirty_bit;
	int8_t page_frame;
}seitentabelle[1024]; // 4194304 >> 12 = 1024

uint16_t get_seiten_nr(uint32_t virt_address) {
	/**
	 *
	 */
	return virt_address >> 12;
}

uint16_t virt_2_ram_address(uint32_t virt_address) {
	/**
	 * Wandelt eine virtuelle Adresse in eine physikalische Adresse um.
	 * Der Rückgabewert ist die physikalische 16 Bit Adresse.
	 */
    int8_t page_frame = seitentabelle[get_seiten_nr(virt_address)].page_frame;

    // Suche nach einem "zufälligen" freien page_frame sollte noch keiner vorhanden sein
    // die frames für ra_mem
    uint8_t ram_present_frames[16];
    if(page_frame == -1) {
        for (size_t i = 0; i < 1024; i++) {
            if(seitentabelle[i].present_bit == 1) {
                ram_present_frames[seitentabelle[i].page_frame] = 1;
            }
        }
        for (int i = 0; i < 16; i++) {
            if (ram_present_frames[i] == 0) {
                page_frame = i;
                break;
            }
        }
        page_frame = (int8_t)((virt_address / 65535) / 16);
    }

    // Variable um zu überprüfen ob der Wert an der physikalischen hd_mem Addresse
    // an die richtige ra_mem Addresse kopiert wird
    cross_value = *((uint8_t*)(hd_mem + (uint32_t)(virt_address / 4096) * 4096) + (virt_address & 0b0000111111111111));

    // Ausgabe eines Index mit dem eine virtuelle Addresse einer ra_mem Addresse zugeordnet werden kann
    return (uint16_t)(page_frame << 12) | (virt_address & 0b111111111111);
}


int8_t check_present(uint32_t virt_address) {
    /**
	 * Wenn eine Seite im Arbeitsspeicher ist, gibt die Funktion "check_present" 1 zurück, sonst 0
	 */
	return seitentabelle[get_seiten_nr(virt_address)].present_bit;
}

uint16_t get_ra_mem_page_start(uint32_t virt_address) {
    /**
     * Errechnet den Index des ersten Elements des Pageframes in ra_mem abhängig von der virtuellen Addresse
     */
    return (uint16_t)(virt_2_ram_address(virt_address) / 4096) * 4096;
}

uint32_t get_hd_mem_page_start(uint32_t virt_address) {
    /**
     * Errechnet den Index des ersten Elements des Pageframes in hd_mem abhängig von der virtuellen Addresse
     */
    return (uint32_t)(virt_address / 4096) * 4096;
}

int8_t is_mem_full() {
	/**
	 * Wenn der Speicher voll ist, gibt die Funktion 1 zurück;
	 */
    // Iteriert über die gesamte Seitentabelle und prüft ob eine Seite in ra_mem liegt, da insgesamt 16 Seiten
    // in ra_mem passen muss also der Speicher voll sein sobald 16 present_bits = 1 gefunden wurden
    size_t ra_mem_ctr = 0;
	for(int i = 0; i < 1024; i++) {
        if(ra_mem_ctr >= 16) {
            return 1;
        }
        else if(seitentabelle[i].present_bit == 1) {
            ra_mem_ctr++;
        }
    }
    return 0;
}

int8_t write_page_to_hd(uint32_t seitennummer, uint32_t virt_address) { // alte addresse! nicht die neue!
	/**
	 * Schreibt eine Seite zurück auf die HD
	 */
    uint32_t hd_mem_page_start = get_hd_mem_page_start(virt_address);
    uint16_t ra_mem_page_start = get_ra_mem_page_start(virt_address);

    // Übersetzen der ra_mem Addressen in dazugehörige hd_mem Addressen und kopieren der Werte im Pageframe
    for (size_t i = ra_mem_page_start, offset = 0; i < ra_mem_page_start + 4096; i++, offset++) {
        hd_mem[hd_mem_page_start + offset] = ra_mem[i];
    }

    // Erstellen eines Seitentabelleneintrags
    seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 0;
    seitentabelle[get_seiten_nr(virt_address)].present_bit = 0;
    seitentabelle[get_seiten_nr(virt_address)].page_frame = (int8_t)(hd_mem_page_start / 4096);

    return seitentabelle[get_seiten_nr(virt_address)].page_frame;
}

uint16_t swap_page(uint32_t virt_address) {
    /**
	 * Das ist die Funktion zur Auslagerung einer Seite.
	 * Wenn das "Dirty_Bit" der Seite in der Seitentabelle gesetzt ist,
	 * muss die Seite zurück in den hd_mem geschrieben werden.
	 * Welche Rückschreibstrategie Sie implementieren möchten, ist Ihnen überlassen.
	 */
	if(seitentabelle[get_seiten_nr(virt_address)].dirty_bit == 1) {
		int8_t d = write_page_to_hd(get_seiten_nr(virt_address), virt_address);
	}

    // Ermittel des nächsten freien Seitenrahmen
	int8_t page_frame = -1;
	for (int i = 0; i < 1024; i++) {
		if (!seitentabelle[i].present_bit) {
			page_frame = i;
			break;
		}
	}
    // Sollte keine Seite geladen un in hd_mem sein muss eine Seite aus ra_mem ausgewählt werden
    // die dann ausgelagert wird
	if (page_frame == -1) {
		int8_t swap_page = -1;
		for (int i = 0; i < 1024; i++) {
			if (seitentabelle[i].dirty_bit == 0 && seitentabelle[i].present_bit == 1) {
				swap_page = i;
				break;
			}
		}
		if (swap_page == -1) {
			return -1;
		}

        //Auszulagernde Seite wird in hd_mem geschrieben
		//int8_t d = write_page_to_hd(swap_page, get_seiten_nr(virt_address));
		int8_t d = write_page_to_hd(swap_page, virt_address);
		page_frame = swap_page;
	}

    //Seitentabelle wird erneuert
	seitentabelle[get_seiten_nr(virt_address)].present_bit = 0;
	seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 0;
	seitentabelle[get_seiten_nr(virt_address)].page_frame = page_frame;

	return page_frame;
}

int8_t get_page_from_hd(uint32_t virt_address) {
	/**
	 * Lädt eine Seite von der Festplatte und speichert diese Daten im ra_mem (Arbeitsspeicher).
	 * Erstellt einen Seitentabelleneintrag.
	 * Wenn der Arbeitsspeicher voll ist, muss eine Seite ausgetauscht werden.
	 */

    uint32_t hd_mem_page_start = get_hd_mem_page_start(virt_address);
    uint16_t ra_mem_page_start = get_ra_mem_page_start(virt_address);

    if(is_mem_full())
        swap_page(virt_address);

    // Übersetzen der hd_mem Addressen in dazugehörige ra_mem Addressen und kopieren der Werte im Pageframe
    for (size_t i = hd_mem_page_start, offset = 0; i < hd_mem_page_start + 4096; i++, offset++)
        ra_mem[ra_mem_page_start + offset] = hd_mem[i];

    // Erstellen eines Seitentabelleneintrags
    seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 0;
    seitentabelle[get_seiten_nr(virt_address)].present_bit = 1;
    seitentabelle[get_seiten_nr(virt_address)].page_frame = (int8_t)(ra_mem_page_start / 4096);

    return seitentabelle[get_seiten_nr(virt_address)].page_frame;
}

uint8_t get_data(uint32_t virt_address) {
    /**
	 * Gibt ein Byte aus dem Arbeitsspeicher zurück.
	 * Wenn die Seite nicht in dem Arbeitsspeicher vorhanden ist,
	 * muss erst "get_page_from_hd(virt_address)" aufgerufen werden. Ein direkter Zugriff auf hd_mem[virt_address] ist VERBOTEN!
	 * Die definition dieser Funktion darf nicht geaendert werden. Namen, Parameter und Rückgabewert muss beibehalten werden!
	 */
    uint16_t ra_mem_index;
	if(check_present(virt_address))
        get_page_from_hd(virt_address);

    // Updaten des letzten Wertes der an der Virtuellen Addresse gespeichert war
     ra_mem_index = virt_2_ram_address(virt_address);

    if(cross_value == ra_mem[ra_mem_index])
        return ra_mem[ra_mem_index];
}

void set_data(uint32_t virt_address, uint8_t value) {
	/**
	 * Schreibt ein Byte in den Arbeitsspeicher zurück.
	 * Wenn die Seite nicht in dem Arbeitsspeicher vorhanden ist,
	 * muss erst "get_page_from_hd(virt_address)" aufgerufen werden. Ein direkter Zugriff auf hd_mem[virt_address] ist VERBOTEN!
	 */
    if(check_present(virt_address) == 0) {
        int8_t d = get_page_from_hd(virt_address);
    }

    uint16_t physical_address = virt_2_ram_address(virt_address);
    uint16_t offset = physical_address & 0b0000111111111111;
    uint16_t ra_mem_index = get_ra_mem_page_start(virt_address) + offset;

    ra_mem[ra_mem_index] = value;
    seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 1;

    uint16_t d = swap_page(virt_address);
}

// Funktionen die das Debuggen erleichtert haben
void print_hd_mem(size_t from, size_t to) {
    for(size_t i = from; i < to; i++) {
        printf("%i ", hd_mem[i]);
    }
    puts("");
}

void print_ra_mem(size_t from, size_t to) {
    for(size_t i = from; i < to; i++) {
        printf("%i ", ra_mem[i]);
    }
    puts("");
}

int main(void) {
	puts("test driver_");
	static uint8_t hd_mem_expected[4194304];
	srand(1);
	fflush(stdout);
	for(int i = 0; i < 4194304; i++) {
		//printf("%d\n",i);
		uint8_t val = (uint8_t)rand();
		hd_mem[i] = val;
		hd_mem_expected[i] = val;
	}

	for (uint32_t i = 0; i < 1024;i++) {
//		printf("%d\n",i);
		seitentabelle[i].dirty_bit = 0;
		seitentabelle[i].page_frame = -1;
		seitentabelle[i].present_bit = 0;
	}

    //print_hd_mem(4191425, 4193425);

	uint32_t zufallsadresse = 4192425;
	uint8_t value = get_data(zufallsadresse);
//	printf("value: %d\n", value);

	if(hd_mem[zufallsadresse] != value) {
		printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
	}

	value = get_data(zufallsadresse);

	if(hd_mem[zufallsadresse] != value) {
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);

	}

//		printf("Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);


	srand(3);

	for (uint32_t i = 0; i <= 1000;i++) {
		uint32_t zufallsadresse = rand() % 4194304;//i * 4095 + 1;//rand() % 4194303
		uint8_t value = get_data(zufallsadresse);
		if(hd_mem[zufallsadresse] != value) {
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
			for (uint32_t i = 0; i <= 1023;i++) {
				//printf("%d,%d-",i,seitentabelle[i].present_bit);
				if(seitentabelle[i].present_bit) {
					printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
				    fflush(stdout);
				}
			}
			exit(1);
		}
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem[zufallsadresse]);
		fflush(stdout);
	}

	srand(3);

	for (uint32_t i = 0; i <= 100;i++) {
		uint32_t zufallsadresse = rand() % 4095 *7;
		uint8_t value = (uint8_t)zufallsadresse >> 1;
		set_data(zufallsadresse, value);
		hd_mem_expected[zufallsadresse] = value;
		//printf("i : %d set_data address: %d - %d value at ram: %d\n",i,zufallsadresse,(uint8_t)value, ra_mem[virt_2_ram_address(zufallsadresse)]);
	}

	srand(4);
	for (uint32_t i = 0; i <= 16;i++) {
		uint32_t zufallsadresse = rand() % 4194304;//i * 4095 + 1;//rand() % 4194303
		uint8_t value = get_data(zufallsadresse);
		if(hd_mem_expected[zufallsadresse] != value) {
			//printf("ERROR_ at Address %d, Value %d =! %d! =! %d\n",zufallsadresse, hd_mem[zufallsadresse], value, hd_mem_expected[zufallsadresse]);
			//print_hd_mem(948725, 950725);
			for (uint32_t i = 0; i <= 1023;i++) {
				//printf("%d,%d-",i,seitentabelle[i].present_bit);
				if(seitentabelle[i].present_bit) {
					printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
				    fflush(stdout);
				}
			}

			exit(2);
		}
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem[zufallsadresse]);
		fflush(stdout);
	}

	srand(3);
	for (uint32_t i = 0; i <= 2500;i++) {
		uint32_t zufallsadresse = rand() % (4095 *5);//i * 4095 + 1;//rand() % 4194303
		uint8_t value = get_data(zufallsadresse);
		if(hd_mem_expected[zufallsadresse] != value ) {
			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem_expected[zufallsadresse], value);
            //printf("present: %d in frame %d is dirty: %d\n", seitentabelle[get_seiten_nr(zufallsadresse)].present_bit, seitentabelle[get_seiten_nr(zufallsadresse)].page_frame, seitentabelle[get_seiten_nr(zufallsadresse)].dirty_bit);
            //print_hd_mem(4775, 6775);
            //print_ra_mem(6 * 4096, (6 * 4096) + 4096);
			for (uint32_t i = 0; i <= 1023;i++) {
				//printf("%d,%d-",i,seitentabelle[i].present_bit);
				if(seitentabelle[i].present_bit) {
					printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
				    fflush(stdout);
				}
			}
			exit(3);
		}
		//printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem_expected[zufallsadresse]);
		fflush(stdout);
	}

	puts("test end");
	fflush(stdout);
	return EXIT_SUCCESS;
}
