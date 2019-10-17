#include <stdio.h>

// Definieren Sie ein enum cardd
enum cardd{
	N=1, E=2, S=4, W=8
};

// Definieren Sie ein 3x3-Array namens map, das Werte vom Typ cardd enthält
enum cardd map[3][3];

// Die Funktion set_dir soll an Position x, y den Wert dir in das Array map eintragen
// Überprüfen Sie x und y um mögliche Arrayüberläufe zu verhindern
// Überprüfen Sie außerdem dir auf Gültigkeit
void set_dir (int x, int y, enum cardd dir)
{
	if (x < 0 || x > 2 || y < 0 || x > 2 || !dir){
		return;
	}
	map[x][y] = dir;
}

// Die Funktion show_map soll das Array in Form einer 3x3-Matrix ausgeben
void show_map (void)
{
	for (int row = 0; row < 3; row++){
		for (int column = 0; column < 3; column++){
			enum cardd value = map[row][column];
			int combined = 0;
			
			switch (value)
			{
			case N|W:
				printf("NW");
				combined = 1;
				break;
			case S|W:
				printf("SW");
				combined = 1;
				break;
			case S|E:
				printf("NE");
				combined = 1;
				break;
			case N|E:
				printf("SE");
				combined = 1;
				break;
			case N:
				printf("N");
				break;
			case S:
				printf("S");
				break;
			case W:
				printf("W");
				break;
			
			case E:
				printf("E");
				break;
			
			default:
				printf("0");
				break;
			}
			if (column < 2){
				if (combined){
					printf("  ");
				}else {
					printf("   ");
				}
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
