/* A program to solve the hotplate problem in a parallel fashion having a near 
 * linear speed up as thread increase
 
 Author: Bukola Grace Omotoso
 MNumber: M01424979
 ID: bgo2e
 Last Code Clean-up: 09/24/2018, 2:20am
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#define MAX_NUM_THREAD 10000
pthread_barrier_t barr;

float max_diffs[MAX_NUM_THREAD];
float** hotplate;
float** hotplateClone;

struct ThreadData {
    int num_rows;
    int num_cols;
    int start_row;
    int stop_row;
    float epsilon;
    int thread_id;
    int thread_num;
};


/* Build memory structure for hotplate*/
float** buildHotplate(int rows, int columns) {
    float** hotplate;
    hotplate = (float**) malloc(rows*sizeof(float*));
    for (int i = 0; i < rows; i++)
        hotplate[i] = (float*) malloc(columns*sizeof(float));
    return hotplate;
}

void initializeHotPlate(int num_rows, int num_cols, float** hotplate, float** hotplateClone, int top_temp, int left_temp, int right_temp, int bottom_temp)    {
    int num_outer_grid = (2 * num_rows) + (2 * (num_cols - 2));
    float outer_grid_sum = (top_temp * (num_cols - 2)) + (left_temp * (num_rows - 1)) + (bottom_temp * num_cols) + (right_temp * (num_rows - 1));
    float initial_inner_val = outer_grid_sum / num_outer_grid;
    
    for (int row = 0; row < num_rows; row++) {
        for (int column = 0; column < num_cols; column++) {
            
            //top values override the top row except the edges
            if ((row == 0) & (column != 0 & column != num_cols - 1)) {
                hotplate[row][column] = top_temp;
                hotplateClone[row][column] = top_temp;
            }
            else if (column == 0 && (row != (num_rows-1))) {
                hotplate[row][column] = left_temp;
                hotplateClone[row][column] = left_temp;
            }
            else if (column == (num_cols - 1) && (row != (num_rows-1))) {
                hotplate[row][column] = right_temp;
                hotplateClone[row][column] = right_temp;
            }
            else if(row == (num_rows -1 )){
                hotplate[row][column] = bottom_temp;
                hotplateClone[row][column] = bottom_temp;
            }
            if ((row != 0) && (row != num_rows - 1) && (column != 0) && (column != num_cols - 1))
                hotplate[row][column] = initial_inner_val;
        }
    }
    
}

/*Get the maximum values from all threads*/
float max_max_diff(float arr[], int n)
{
    int i;
        float max = arr[0];
    for (i = 1; i < n; i++)
        if (arr[i] > max)
            max = arr[i];
    return max;
}

/* Swap hotplate and its clone*/

void swapHotplate(float *a, float *b) {
    
    float tmp = *a;
    *a = *b;
    *b = tmp;
}

/* Get current time*/

double timestamp()
{
    struct timeval tval;
    
    gettimeofday( &tval, ( struct timezone * ) 0 );
    return ( tval.tv_sec + (tval.tv_usec / 1000000.0) );
}

/* Heating up hotplate*/

void *generateHeat(void *arguments) {
    
    struct ThreadData *data = arguments;
    int num_rows = data -> num_rows;
    int num_cols = data -> num_cols;
    int start_row = data -> start_row;
    int stop_row = data -> stop_row;
    float epsilon = data -> epsilon;
    int thread_id = data -> thread_id;
    int thread_num = data ->thread_num;
    float *value = (float *)malloc(sizeof(float));
    
    float max_difference = 0;
    float previous_val;
    float current_val;
    float diff;
    
    alarm(3600);
    for (int row = start_row; row < stop_row; row++) {
        for (int column = 1; column < (num_cols - 1); column++) {
            previous_val = hotplate[row][column];
            current_val = ((hotplate[row - 1][column] + hotplate[row][column - 1] + hotplate[row + 1][column] + hotplate[row][column + 1]) / 4);
            diff = fabsf(previous_val - current_val);
            if (diff > max_difference){
                max_difference = diff;
            }
            hotplateClone[row][column] = current_val;
            
        }
        
    }
    max_diffs[thread_id] = max_difference;
    pthread_barrier_wait(&barr);
    *value = max_max_diff(max_diffs,thread_num);
    pthread_exit(value);
}


int main(int argc, char const *argv[])
{
    int num_rows = atoi(argv[1]);
    int num_cols = atoi(argv[2]);
    int top_temp = atoi(argv[3]);
    int left_temp = atoi(argv[4]);
    int right_temp = atoi(argv[5]);
    int bottom_temp = atoi(argv[6]);
    float epsilon = atof(argv[7]);
    int num_threads = atoi(argv[8]);
    
    
    hotplate =  buildHotplate(num_rows, num_cols);
    hotplateClone = buildHotplate(num_rows, num_cols);
    
    initializeHotPlate(num_rows, num_cols, hotplate, hotplateClone, top_temp, left_temp, right_temp, bottom_temp);
    pthread_t threads[num_threads];
    printf("%-10s%10s\n", "Iteration", "Epsilon");
    struct ThreadData data[num_threads];
    int  row_per_thread = (num_rows + num_threads - 1)/num_threads;
    
    //divide the work among the available threads
    for(int i = 0; i < num_threads; i++)    {
        data[i].start_row = i*row_per_thread;
        data[i].stop_row = (i + 1)*row_per_thread;
        data[i].num_rows = num_rows;
        data[i].num_cols = num_cols;
        data[i].epsilon = epsilon;
        data[i].thread_id = i;
        data[i].thread_num = num_threads;
    }
    
    /*Ensure that the first thread starts at row 1 and 
     * the last thread does not go past the end of a column*/
     
    data[0].start_row = 1;
    data[num_threads-1].stop_row = num_rows-1;
    
    void *vptr_return;
    float max_difference = epsilon + 1;
    int counter = 0;
    double begin, end;
    
    pthread_barrier_init(&barr,NULL,num_threads);
    begin = timestamp();
    while(max_difference > epsilon){
        for (int i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], NULL, &generateHeat, (void *)&data[i]);
        }
        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], &vptr_return);
        }
        swapHotplate((float*)&hotplate, (float*)&hotplateClone);
        max_difference = *((float *)vptr_return);
        free(vptr_return);
        if ((counter > 0 && (counter & (counter - 1)) == 0 )|| max_difference < epsilon)
            printf("%-10d%10.6f\n", counter, max_difference);
        counter++;
    }
    end = timestamp();
    
    printf("%s%5.2f\n","TOTAL TIME: ", (end-begin));
    return 0;
}
