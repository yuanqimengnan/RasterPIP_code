#ifndef FOREXPERIMENT_H
#define FOREXPERIMENT_H
#include <string>
#include <math.h>
#include <Common.h>

using namespace std;

//To calculate time
extern string title;
extern float_t  experiment_time[10];
enum Dataset1 {yellow01=1,yellow02,yellow03,yellow04,yellow05,yellow06,yellow07,yellow08,yellow09,yellow10,yellow11,yellow12,twitter,gowalla,second,third,forth,fifth,sixth,seventh,eighth,ninth,tenth,eleventh,twelfth};
extern int dataset;
#endif // FOREXPERIMENT_H

//for group by
extern int batch_size;
extern int single_pass_size;
