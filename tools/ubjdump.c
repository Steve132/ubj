#include<stdio.h>
#include<ubj.h>



void print_help()
{
	
}

int main(int argc,char** argv)
{
	FILE* fp;
	if(argc > 1)
	{
		if(argv[1][0]=='-' && argv[1][1]=='-')
		{
			fp=stdin;
		}
		else
		{
			fp=fopen(argv[1],"rb");
		}
	}
	else
	{
		print_help();
		return 0;
	}
	ubjr_context_t* ubctx=ubjr_open_file(fp);
	ubjr_dynamic_t dyn=ubjr_read_dynamic(ubctx);
	
	return 0;
}