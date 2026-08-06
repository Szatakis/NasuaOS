bool strcmp(const char* a, const char* b)
{
    while(*a && *b)
    {
        if(*a != *b)
        {
            return false;
        }

        a++;
        b++;
    }

    return *a == 0 && *b == 0;
}

bool contains(const char* text, const char* find)
{
    while(*text)
    {
        const char* a = text;
        const char* b = find;


        while(*a && *b && *a == *b)
        {
            a++;
            b++;
        }


        if(*b == 0)
            return true;


        text++;
    }


    return false;
}