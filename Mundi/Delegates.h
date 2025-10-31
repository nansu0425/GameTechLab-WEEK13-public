#pragma once

template<typename... Args>
class TDelegate
{
public:
	using HandlerType = std::function<void(Args...)>;

	void Add(const HandlerType& Handler)
	{
		Handlers.push_back(Handler);
	}

	template<typename T>
	void AddDynamic(T* Instance, void(T::*Func)(Args...))
	{
		Handlers.push_back([=](Args... args) {
			(Instance->*Func)(args...);
		});
	}

	void Broadcast(Args... args) {
		for (auto& Handler : Handlers) {
			Handler(args...);
		}
	}
	
private:
	std::vector<HandlerType> Handlers;
};

#define DECLARE_DELEGATE(Name, ...)				TDelegate<__VA_ARGS__> Name
#define DECLARE_DELEGATE_OneParam(Name, T1)		TDelegate<T1> Name
#define DECLARE_DELEGATE_TwoParam(Name, T1, T2)	TDelegate<T1, T2> Name
#define DECLARE_DYNAMIC_DELEGATE(Name, ...)		std::shared_ptr<TDelegate<__VA_ARGS__>> Name = std::make_shared<TDelegate<__VA_ARGS__>>();