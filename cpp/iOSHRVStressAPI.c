#include "iOSHRVStressAPI.h"
#include <math.h>

double computeGaussianCDF(double x, double mean, double sd)
{
    return 0.5 * (1 + erf((x - mean) / sd / sqrt(2)));
}

int get_percentage_stress(double rMSSD, int age)
{
    int percentage_rank = 100;
    if (age <= 29)
    {
        percentage_rank = 100 * computeGaussianCDF(rMSSD, 72.39, 47.10);
    }
    else if (age >= 50)
    {
        percentage_rank = 100 * computeGaussianCDF(rMSSD, 40.40, 18.29);
    }
    else
    {
        percentage_rank = 100 * computeGaussianCDF(rMSSD, 48.40, 20.90);
    }
    return percentage_rank;
}
