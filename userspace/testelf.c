/*
 * testelf.c
 *
 */

void func2(int a) {
    asm("int $3");
    return;
}

void func1(int a, int b) {
    func2(a);
}


int main() {
    int x = 1;
    int a = 5*x;
    a = a-1;
    func1(a, x);
    while (1);
}


