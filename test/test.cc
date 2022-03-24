#include<iostream>
#include<sstream>
#include<string>
#include<algorithm>

int main()
{
	std::string str = "GET /x/y/z HTTP/1.1";
	std::cout<<str<<std::endl;
	transform(str.begin(),str.end(),str.begin(),toupper);
	std::cout<<str<<std::endl;
}

//int main()
//{
//	std::string str = "i am zhangyi";
//	std::stringstream ss(str);
//
//	std::string a,b,c;
//	ss >> a >> b >> c;
//
//	std::cout << a << std::endl;
//	std::cout << b << std::endl;
//	std::cout << c << std::endl;
//
//	return 0;
//}
