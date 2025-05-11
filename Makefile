default:
	# compile and archive library
	gcc -O2 -fPIC -static -c lib/arg_parser.c -o lib/arg_parser.o
	ar rcs lib/libargparser.a lib/arg_parser.o
	rm lib/arg_parser.o

	# compile and link jump_edit
	gcc -O2 -Iinclude jump_edit.c -Llib -lgdbm -largparser  -o jump_edit 

