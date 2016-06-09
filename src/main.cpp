#include <QApplication>
#include <stdio.h>
#include "v4lcamera.h"

int main(int argc, char *argv[])
{
	setvbuf(stdout,(char*)NULL,_IONBF,0);

	QApplication a(argc, argv);
	v4lcamera w(NULL,0);
	w.show();

  return a.exec();
}
