// pdal-session.cpp
// Abstract and maintain a PDAL session (Eventually)
//

#include <json/json.h>

#include <stdexcept>
#include <iostream>
#include <map>
#include <cstdlib>
#include <memory>
#include <functional>
#include <type_traits>

#include <thread>
#include <chrono>

#include <boost/algorithm/string.hpp>
#include <boost/shared_array.hpp>
#include <boost/asio.hpp>

#include <pdal/PipelineReader.hpp>
#include <pdal/PipelineManager.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/PointBuffer.hpp>
#include <pdal/StageIterator.hpp>

/**
 * @brief		The KeyMaker class helps adapt a Key used in CommandManager to a unique key
 *				for internal indexing purposes.  This class needs to be specialized for any
 *				key type you may want to use.  The default implementation for strings is provided.
 *
 * @tparam T	The type of the key.
 */
template<typename T>
struct KeyMaker {
	static_assert(sizeof(T) == 0, "You need to specialize this class for your key-type");

	/**
	 * @brief		Take an object of type T and return a key representation of it.
	 *
	 * @param t		The object to convert.
	 *
	 * @returns		The key representation of t
	 */
	static T key(const T& t) { }
};

/**
 * @brief		The reader/writer adaptor to read and write typed objects of type T from given
 *				streams.  This class needs to be specialized for any key type you want to use.
 *
 * @tparam T	The type of object to read and write
 */
template<typename T>
struct ReaderWriter {
	static_assert(sizeof(T) == 0, "You need to specialize this class for your key-type");

	/**
	 * @brief			Read an object from the given stream.
	 *
	 * @param stream	The stream to read the object from.
	 * @param t			The object passed in as reference that will be read into.
	 *
	 * @returns			true if read was successful, false otherwise.
	 */
	bool read(std::istream& stream, T& t) { return false; }


	/**
	 * @brief			Write an object to the given output stream.
	 *
	 * @param stream	The stream to write to
	 * @param t			The object to write.
	 */
	void write(std::ostream& stream, const T& t) { }
};


/**
 * @brief			A simple way to associate keys with callable objects which deal with json objects.
 *
 * @tparam TKey		The key type.
 * @tparam F		The callable type.
 * @tparam keymaker	The keymaker with type TKey used to convert Key objects to Keys.
 */
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

		/**
		 * @brief			Dispatch the callable associated with the given command using
		 *					the specified parameter.
		 *
		 * @tparam V		The type of parameter passed to callable.
		 * @param command	The command to call, basically the object with which a callable was associated
		 * @param v			The object to pass to the callable
		 *
		 * @returns			The return value returned by the callable, a json value.
		 */
		template<typename V>
		Json::Value dispatch(const TKey& command, const V& v) const {
			try {
				const_iterator iter = commands_.find(keymaker::key(command));
				if (iter == commands_.end())
					throw std::runtime_error("Unknown command");

				Json::Value vRet = (*iter).second(v);
				vRet["status"] = 1;

				return vRet;
			}
			catch(std::runtime_error& e) {
				Json::Value ex;
				ex["status"] = 0;
				ex["message"] = e.what();

				return ex;
			}
		}

	private:
		/**
		 * @brief		Return a unit (object with single key) with the given key and value.
		 *
		 * @tparam T	The type of value for the given key.
		 * @param s		The key name.
		 * @param v		The value.
		 *
		 * @returns		A unit json value which s => v mapping.
		 */
		template<typename T>
		static Json::Value unit(const std::string& s, const T& v) {
			Json::Value r;
			r[s] = v;
			return r;
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
		// Json::Reader supports directly reading from a stream but it expects
		// the stream to end with an EOF indicator before it will try to parse
		// it.  We need to continuously parse whatever top-level objects we can,
		// so we have this in place to account for that.
		//
		std::stringstream sstr;
		std::stack<int> stk;

		while(s.good()) {
			char c;

			s.get(c);
			sstr << c;

			if (c == '{') stk.push(0);
			if (c == '}') {
				if (stk.size() == 0)
					return false; // mismatched brace?

				stk.pop();
				if (stk.size() == 0) {
					reader.parse(sstr.str(), v);
					sstr.str("");

					return true;
				}
			}
		}

		return false;
	}

	void write(std::ostream& s, const Json::Value& v) {
		s << v.toStyledString() << std::endl;
	}
};


/**
 * @brief				A simple class to abstract reading and writing from/to streams.
 *
 * @tparam T			The type of object that will be read and written.
 * @tparam ReadWrite	The ReaderWriter adaptor for type T
 */
template<typename T, class ReadWrite = ReaderWriter<T> >
class IO {
public:
	/**
	 * @brief	Constructor. Setup IO object with the given input/output streams.
	 *
	 * @param sin
	 * @param sout
	 */
	IO(std::istream& sin, std::ostream& sout) : sin_(sin), sout_(sout) {
	}

	/**
	 * @brief	Sets up a reader/writer loop where objects are read and passed to the provided callable. The
	 *			loop keeps going till the read call returns false.
	 *
	 * @tparam F	The type of the callable
	 * @param f		The callable.
	 */
	template<typename F>
	void forInput(F f) {
		T t;
		ReadWrite rw; 

		while(rw.read(sin_, t)) {
			rw.write(sout_, f(t));
		}
	}

	/**
	 * @brief  Write the given value to out stream
	 *
	 * @param val The value to write
	 */
	void write(const T& val) {
		ReadWrite rw;
		rw.write(sout_, val);
	}

private:
	std::istream& sin_;
	std::ostream& sout_;
};

/**
 * @brief	A session manager for an object which adheres with the PDAL object spec
 *
 * @tparam T	The type of the object for which the sessions are being maintained.
 */
template<typename T>
class SessionManager  {
public:
	typedef T contained_type;
	typedef std::shared_ptr<T> shared_ptr;

public:
	SessionManager() : p_() { }

	/**
	 * @brief		Create a new instance of type T
	 *
	 * @param desc	A pipeline description passed to T's constructor
	 */
	void create(Json::Value const& params) {
		p_.reset(new T(params));
	}

	/**
	 * @brief  Destroy the contained instance
	 */
	void destroy() {
		p_.reset();
	}

	/**
	 * @brief  Get the number of points returned by the desc passed to create.
	 *
	 * @return	The total number of points.
	 */
	size_t getNumPoints() const {
		if (!p_) throw std::runtime_error("Session is not valid");
		return p_->getNumPoints();
	}

    inline std::string getSRS() const { return "the coordinate system; ";}


	/**
	 * @brief  Get the total size of buffer needed to hold all points
	 *
	 * @return	The total size of buffer in bytes.
	 */
	size_t stride() const {
		if (!p_) throw std::runtime_error("Session is not valid");
		return p_->stride();
	}

	/**
	 * @brief		Is the contained instance valid/initialized?
	 *
	 * @returns		true if yes, false otherwise.
	 */
	bool isValid() const { 
		return !!p_;
	}

	/**
	 * @brief		Read the given number of points starting at a given offset into the provided buffer
	 *
	 * @param buf			The buffer to read into.
	 * @param startIndex	The start offset
	 * @param npoints		The number of points to read
	 *
	 * @returns		Number of points actually read, could be <= npoints.
	 */
	size_t read(void *buf, size_t startIndex, size_t npoints) {
		if (!p_) throw std::runtime_error("Session is not valid");
		return p_->read(buf, startIndex, npoints);
	}
    
    void initialize()
    {
        p_->initialize();
    }

private:
	shared_ptr p_;
};

struct DummyPDAL {
	DummyPDAL(Json::Value const& params) {
		std::srand(std::time(NULL));
		points_ = (std::rand() % 10000) + 5000;
	}
    
    void initialize() {};
	size_t getNumPoints() const { return points_; }
	size_t stride() const { return sizeof(float) * 4; }
    
	size_t read(void *buf, size_t startIndex, size_t npoints) {
		if (startIndex >= points_)
			throw std::runtime_error("startIndex cannot be more than the total number of points");

		npoints = startIndex + npoints > points_ ? points_ - startIndex : npoints;

		float *p = reinterpret_cast<float*>(buf);
		std::fill(p, p + npoints, 0.0f);
		return npoints;
	}


	size_t points_;
};



struct RealPDAL {
	RealPDAL(Json::Value const& params) : stage(0), buffer(0), iterator(0), params(params)
    {

	}
    
    ~RealPDAL()
    {
        if (iterator)
            delete iterator;
        

        // 
        // if (stage)
        //     delete stage;
        if (buffer)
            delete buffer;        
        
    }
    
    void initialize()
    {
        bool debug = params["debug"].asBool();
        int verbose = params["verbose"].asInt();
        pdal::PipelineReader reader(manager, debug, verbose);
        std::string filename = params["filename"].asString();

        reader.readPipeline(filename);
        stage = manager.getStage();
        stage->initialize();

		// create a point buffer to read in points
		pdal::PointBuffer *pbuf =
			new pdal::PointBuffer(stage->getSchema(), stage->getNumPoints());

        iterator = stage->createSequentialIterator(*pbuf);
        iterator->read(*pbuf);

		// right now we're only interested in the output of our pipeline, so
		// pack data
		buffer = pbuf->pack();
		delete pbuf;
        

		// just make sure that our final buffer contains the X Y and Z dimenions
		//
		try {
			const pdal::Schema& s = buffer->getSchema();

			s.getDimension("X");
			s.getDimension("Y");
			s.getDimension("Z");
		}
		catch (pdal::dimension_not_found&) {
			throw std::runtime_error("Pipeline output should contain X, Y and Z dimensions");
		}
    }

    size_t getNumPoints() const 
    {
        return stage->getNumPoints(); 
    }
    size_t stride() const 
    {
        return buffer->getSchema().getByteSize();
    }
    //     
    size_t read(void *buf, size_t startIndex, size_t npoints) {
        if (startIndex >= stage->getNumPoints())
            throw std::runtime_error("startIndex cannot be more than the total number of points");
    
        npoints = startIndex + npoints > stage->getNumPoints() ? stage->getNumPoints() - startIndex : npoints;
        
        boost::uint8_t const* start = buffer->getData(startIndex);
        memcpy(buf, start, npoints*stride());

        return npoints;
    }

    
    pdal::Stage* stage;
    pdal::PipelineManager manager;
    pdal::PointBuffer* buffer;
    pdal::StageSequentialIterator* iterator;
    Json::Value params;
    
private:
    RealPDAL(); 
};

/**
 * @brief	A callable transmitter which transmits given buffer over to the specified
 *			host and port.
 */
class BufferTransmitter {
	public:
		/**
		 * @brief			Construct a buffer transmitter callable
		 *
		 * @param host		The host to send data to
		 * @param port		The port to send to
		 * @param buffer	The buffer to send
		 * @param nlen		The length of buffer
		 */
		BufferTransmitter(const std::string& host, int port,
			boost::shared_array<char> buffer, size_t nlen) :
			host_(host), port_(port), buf_(buffer), nlen_(nlen) { }

		void operator()() {
			namespace asio = boost::asio;
			using boost::asio::ip::tcp;

			std::stringstream port;
			port << port_;

			asio::io_service service;
			tcp::resolver resolver(service);

			tcp::resolver::query q(host_, port.str());
			tcp::resolver::iterator iter = resolver.resolve(q), end;

			tcp::socket socket(service);

			int retryCount = 0;

			boost::system::error_code ignored_error;

			// Don't bail out on first attempt to connect, the setter upper
			// service may need time to set the reciever
			tcp::resolver::iterator connectIter;
			while((connectIter = asio::connect(socket, iter, ignored_error)) == end && retryCount ++ < 500)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

			// TODO: We need to propagate the error information to the user
			if(connectIter == end)
				return; // no point proceeding, could not connect

			// send our data
			asio::write(socket, asio::buffer(buf_.get(), nlen_), ignored_error);
		}


	private:
		std::string host_;
		int port_;
		boost::shared_array<char> buf_;
		size_t nlen_;
};


int main() {
	Json::Reader reader;
	Json::Value v;

	IO<Json::Value> io(std::cin, std::cout);
	CommandManager<std::string, std::function<Json::Value (const Json::Value&)> > commands;

	SessionManager<RealPDAL> session;

	commands.add("isSessionValid", [&session](const Json::Value&) -> Json::Value {
		Json::Value r;
		r["valid"] = session.isValid();

		return r;
	});

	commands.add("create", [&session](const Json::Value& params) -> Json::Value {
        // std::string desc = params["pipelineDesc"].asString();

		session.create(params);

        try
        {
            session.initialize();

        } catch (std::exception& e)
        {
            Json::Value v;
            v[std::string("error")] = e.what();
            return v;
        }

		return Json::Value();
	});

	commands.add("destroy", [&session](const Json::Value&) -> Json::Value {
		session.destroy();
		return Json::Value();
	});

	commands.add("getNumPoints", [&session](const Json::Value&) -> Json::Value {
		Json::Value v;
		v[std::string("count")] = (int)session.getNumPoints();

		return v;
	});

	commands.add("getSRS", [&session](const Json::Value&) -> Json::Value {
		Json::Value v;
		v[std::string("srs")] = session.getSRS();

		return v;
	});

	commands.add("read", [&session](const Json::Value& params) -> Json::Value {
		size_t npoints = session.getNumPoints();

		size_t start = params.isMember("start") ? params["start"].asInt() : 0;
		size_t count = params.isMember("count") ? params["count"].asInt() : npoints;

		size_t nbufsize = session.stride() * count;

		std::string host = params["transmitHost"].asString();
		int port = params["transmitPort"].asInt();

		boost::shared_array<char> pbuf(new char[nbufsize]);
		count = session.read(pbuf.get(), start, count);

		std::thread t(BufferTransmitter(host, port, pbuf, nbufsize));
		t.detach();

		Json::Value v;
		v["message"] = "Read request queued for points to be delivered to specified host:port";
		v["pointsRead"] = (int)count;
		v["bytesCount"] = (int)nbufsize;

		return v;
	});

	// indicate that we're ready to our controller program
	Json::Value vReady; vReady["ready"] = 1;
	io.write(vReady);

	io.forInput([&commands](const Json::Value& v) -> Json::Value {
		return commands.dispatch(v["command"].asString(), v["params"]);
	});

    


	return 0;
}
