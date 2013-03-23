BEGIN {
  interval = 0;
  dropped=0;
}

{
  
  if ($1 < interval+1) {
    dropped++;
  }
  else {
    while (interval+1 < $1) {
#printf ("dropped between %d and %d: %d\n", interval, ++interval, dropped);
      printf ("%d\t%d\n", ++interval, dropped);
      dropped = 0;
    }
    dropped = 1;
  }
}

END {
  #flush the last info
  printf ("%d\t%d\n", ++interval, dropped);
  while (interval < 170) {
    printf ("%d\t%d\n", ++interval, 0);
  }
	printf ("FINISH\n");
}
