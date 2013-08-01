// pdal-session.cpp
// Abstract and maintain a PDAL session (Eventually)
//

#include <json/json.h>

#include <stdexcept>
#include <iostream>
#include <map>

#include <functional>
#include <type_traits>

#include <boost/algorithm/string.hpp>

template<typename T>
struct KeyMaker {
	static_assert(sizeof(T) == 0, "You need to specialize this class for your key-type");
	static T key(const T& t) { }
};

template<typename T>
struct ReaderWriter {
	static_assert(sizeof(T) == 0, "You need to specialize this class for your key-type");
	bool read(T& t) { return false; }
	void write(const T& t) { }
};


template<typename TKey, typename F, class keymaker = KeyMaker<TKey> >
class CommandManager {
	public:
		typedef F function_type;
		typedef TKey key_type;
		typedef typename std::map<TKey, F> storage_type;
		typedef typename storage_type::const_iterator const_iterator;
		typedef typename storage_type::iterator iterator;

	public:
		CommandManager() : commands_() { }
		void add(const TKey& command, F f) {
			commands_[keymaker::key(command)] = f;
		}

		template<typename V>
		Json::Value dispatch(const TKey& command, const V& v) const {
			try {
				const_iterator iter = commands_.find(keymaker::key(command));
				if (iter == commands_.end())
					throw std::runtime_error("Unknown command");

				return (*iter).second(v);
			}
			catch(std::runtime_error& e) {
				Json::Value ex;
				ex["success"] = 0;
				ex["message"] = e.what();

				return ex;
			}
		}

	private:
		storage_type commands_;
};

template<>
struct KeyMaker<std::string> {
	static std::string key(const std::string& s) {
		return boost::to_lower_copy(s);
	}
};

template<>
struct ReaderWriter<Json::Value> {
	Json::Reader reader;

	bool read(std::istream& s, Json::Value& v) {
		return reader.parse(s, v);
	}

	void write(std::ostream& s, const Json::Value& v) {
		s << v.toStyledString() << std::endl;
	}
};


template<typename T, class ReadWrite = ReaderWriter<T> >
class IO {
public:
	IO(std::istream& sin, std::ostream& sout) : sin_(sin), sout_(sout) {
	}

	template<typename F>
	void forInput(F f) {
		T t;
		ReadWrite rw; 

		while(rw.read(sin_, t)) {
			rw.write(sout_, f(t));
		}
	}
private:
	std::istream& sin_;
	std::ostream& sout_;
};

int main() {
	Json::Reader reader;
	Json::Value v;

	IO<Json::Value> io(std::cin, std::cout);
	CommandManager<std::string, std::function<Json::Value (const Json::Value&)> > commands;


	commands.add("hello", [](const Json::Value&) -> Json::Value {
		Json::Value v;
		v["left"] = 10;
		v["right"] = 20;

		return v;
	});

	commands.add("bye", [](const Json::Value&) -> Json::Value {
		Json::Value v;
		v["message"] = "See you later";

		return v;
	});

	io.forInput([&commands](const Json::Value& v) -> Json::Value {
		return commands.dispatch(v["command"].asString(), v["params"]);
	});

	return 0;
}
