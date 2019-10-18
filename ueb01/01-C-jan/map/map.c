#include <stdio.h>

// Definieren Sie ein enum cardd
typedef enum cardd {LEER,N,E,S,W} cardd;

// Definieren Sie ein 3x3-Array namens map, das Werte vom Typ cardd enthält
enum cardd map[3][3];

// Die Funktion set_dir soll an Position x, y den Wert dir in das Array map eintragen
// Überprüfen Sie x und y um mögliche Arrayüberläufe zu verhindern
// Überprüfen Sie außerdem dir auf Gültigkeit
void set_dir (int x, int y, cardd dir)
{
	if(x < 3 && y < 3) {
	
		map[x][y] = dir;
	}
}

// Die Funktion show_map soll das Array in Form einer 3x3-Matrix ausgeben
void show_map (void)
{
	int numberOfLines = 3;
	int numberOfColumns = 3;

	for(int rows = 0; rows < numberOfLines; rows++) {

		for(int columns = 0; columns < numberOfColumns; columns++) {
	
			switch(map[rows][columns]) {

				case LEER : printf("%s	", "0"); break; 
				case N : printf("%s	", "N"); break; 
				case E : printf("%s	", "E"); break;
				case S : printf("%s	", "S"); break;
				case W : printf("%s	", "W"); break;
				default: printf("geht nicht!");
			} 
		}
		printf("\n");
	}
}

int main (void)
{
	// In dieser Funktion darf nichts verändert werden!
	set_dir(0, 1, N);
	set_dir(1, 0, W);
	set_dir(1, 4, W);
	set_dir(1, 2, E);
	set_dir(2, 1, S);

	show_map();

//	set_dir(0, 0, N|W);
//	set_dir(0, 2, N|E);
//	set_dir(0, 2, N|S);
//	set_dir(2, 0, S|W);
//	set_dir(2, 2, S|E);
//	set_dir(2, 2, E|W);
//	set_dir(1, 3, N|S|E);
//	set_dir(1, 1, N|S|E|W);
//
//	show_map();

	return 0;
}
