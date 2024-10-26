#include <stdlib.h>

static int ft_abs(int c) {
  if (c < 0)
    return (-c);
  return (c);
}

static int ft_ndivs(int n) {
  int res;

  res = 0;
  if (n == 0)
    return (1);
  if (n < 0) {
    res++;
    n *= -1;
  }
  while (n != 0) {
    n /= 10;
    res++;
  }
  return (res);
}

char *ft_itoa(int n) {
  int i;
  char *res;

  i = ft_ndivs(n);
  res = (char *) malloc(sizeof(char) * (i + 1));
  if (!res)
    return (NULL);
  res[i--] = 0;
  if (n == 0)
    res[i] = '0';
  if (n < 0) {
    res[0] = '-';
  }
  while (n != 0) {
    res[i--] = '0' + ft_abs(n % 10);
    n /= 10;
  }
  return (res);
}