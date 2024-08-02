run:
	@gcc main.c -o hu_soft_mmu -fno-stack-protector -g -fsanitize=address -std=c11
	@./hu_soft_mmu
