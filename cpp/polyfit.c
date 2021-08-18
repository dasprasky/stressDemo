#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <errno.h>
#include "polyfit.h"

void polyvals(float x[], float y[], float a[], uint32_t N, uint8_t order)
{
  uint8_t i;
  uint32_t k;
  if(order > POLYNOMIAL_MAX_ORDER) 
  {
    return;
  }
  
  for(k=0;k<N;k++)
  {
    y[k] = a[0];
    for(i=1;i<=order;i++)
    {
      y[k] += a[i] * powf(x[k], (float)i);
    }
  }
}

void polyfit(float x[], float y[], float a[], uint32_t N, uint8_t order)
{
  int8_t i;
  uint32_t j;
  uint8_t k;
  float X[2*POLYNOMIAL_MAX_ORDER+1];                        //Array that will store the values of sigma(xi),sigma(xi^2),sigma(xi^3)....sigma(xi^2n)
  float B[POLYNOMIAL_MAX_ORDER+1][POLYNOMIAL_MAX_ORDER+2];  //B is the Normal matrix(augmented) that will store the equations, 'a' is for value of the final coefficients
  float Y[POLYNOMIAL_MAX_ORDER+1];                          //Array to store the values of sigma(yi),sigma(xi*yi),sigma(xi^2*yi)...sigma(xi^n*yi)

  if(order > POLYNOMIAL_MAX_ORDER) 
  {
    return;
  }
  else
  {
    for(i=0;i<=order;i++)
    {
      a[i] = 0.0f;
    }
  }

  for (i=0;i<2*order+1;i++)
  {
    X[i]=0;
    for (j=0;j<N;j++)
    {
      X[i]=X[i]+powf(x[j],(float)i);        //consecutive positions of the array will store N,sigma(xi),sigma(xi^2),sigma(xi^3)....sigma(xi^2n)
    }
  }
  for (i=0;i<=order;i++)
  {
    for (j=0;j<=order;j++)
    {
      B[i][j]=X[i+j];            //Build the Normal matrix by storing the corresponding coefficients at the right positions except the last column of the matrix
    }
  }
  for (i=0;i<order+1;i++)
  {    
    Y[i]=0;
    for (j=0;j<N;j++)
    {
      Y[i]=Y[i]+powf(x[j],(float)i)*y[j];     //consecutive positions will store sigma(yi),sigma(xi*yi),sigma(xi^2*yi)...sigma(xi^n*yi)
    }
  }
  for (i=0;i<=order;i++)
  {
    B[i][order+1]=Y[i];               //load the values of Y as the last column of B(Normal Matrix but augmented)
  }

  order=order+1;                      //n is made n+1 because the Gaussian Elimination part below was for n equations, but here n is the degree of polynomial and for n degree we get n+1 equations
  for (i=0;i<order;i++)               //From now Gaussian Elimination starts(can be ignored) to solve the set of linear equations (Pivotisation)
  {
    for (k=i+1;k<order;k++)
    {
      if (B[i][i]<B[k][i])
      {
        for (j=0;j<=order;j++)
        {
          float temp=B[i][j];
          B[i][j]=B[k][j];
          B[k][j]=temp;
        }
      }
    }
  }
    
  for (i=0;i<order-1;i++)             //loop to perform the gauss elimination
  {
    for (k=i+1;k<order;k++)
    {
      float t=B[k][i]/B[i][i];
      for (j=0;j<=order;j++)
      {
        B[k][j]=B[k][j]-t*B[i][j];    //make the elements below the pivot elements equal to zero or elimnate the variables
      }
    }
  }
  for (i=order-1;i>=0;i--)            //back-substitution
  {                                   //x is an array whose values correspond to the values of x,y,z..
    a[i]=B[i][order];                 //make the variable to be calculated equal to the rhs of the last equation
    for (j=0;j<order;j++)
    {
      if (j!=i)                       //then subtract all the lhs values except the coefficient of the variable whose value                                   is being calculated
      {
        a[i]=a[i]-B[i][j]*a[j];
      }
    }
    a[i]=a[i]/B[i][i];                //now finally divide the rhs by the coefficient of the variable to be calculated
  }
}
