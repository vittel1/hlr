#include <stdio.h>

// Definieren Sie ein enum cardd
typedef enum {
	N=1, E=2, S=4, W=8
} cardd;

// Definieren Sie ein 3x3-Array namens map, das Werte vom Typ cardd enthält
cardd map[3][3];

// Die Funktion set_dir soll an Position x, y den Wert dir in das Array map eintragen
// Überprüfen Sie x und y um mögliche Arrayüberläufe zu verhindern
// Überprüfen Sie außerdem dir auf Gültigkeit
void set_dir (int x, int y, cardd dir) {
	if((x >= 0 && x <= 2)&&(y >= 0 && y <= 2)) {
		map[x][y] = dir;
	}
}

// Die Funktion show_map soll das Array in Form einer 3x3-Matrix ausgeben
// Bitwise or: 0001|1000 = 1001 -> 9
void show_map (void) {
	
	for (int x = 0; x < 3; x++) {
		for (int y = 0; y < 3; y++) {
			switch(map[x][y]) {
				case 1: printf("N  "); break;
				case 2: printf("E  "); break;
				case 4: printf("S  "); break;
				case 8: printf("W  "); break;
				
				//Pos (0/2). N|S. Case 3 ist unnoetig da ueberschrieben von N|S
				case 5: printf("NE  "); break;

				//Pos: (0/0): N|W: 0001|1000 = 1001 = 9
				case 9: printf("NW  "); break;

				//Pos: (2/2). E|W. 0010|1000 = 1010 = 10. Wird auch überschrieben.
				case 10: printf("SE  "); break;
	
				//Pos (0/2). N|E.
				case 12: printf("SW  "); break;

				default: printf("0  "); break;
			}
		}
		printf("\n");
	}
}

int main (void) {
	// In dieser Funktion darf nichts verändert werden!
	set_dir(0, 1, N);
	set_dir(1, 0, W);
	set_dir(1, 4, W);
	set_dir(1, 2, E);
	set_dir(2, 1, S);

	show_map();

	set_dir(0, 0, N|W);
	set_dir(0, 2, N|E);
	set_dir(0, 2, N|S);
	set_dir(2, 0, S|W);
	set_dir(2, 2, S|E);
	set_dir(2, 2, E|W);
	set_dir(1, 3, N|S|E);
	set_dir(1, 1, N|S|E|W);

	show_map();

	return 0;
}