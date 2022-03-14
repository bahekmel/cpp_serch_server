#pragma once



#include <map>
#include <set>
#include <string>
#include <vector>
#include <mutex>

//====================== ConcurrentMap =========================//

template <typename Key, typename Value>
class ConcurrentMap {
private:
	struct Bucket {
		std::mutex mutex;
		std::map<Key, Value> map;
	};

public:
	static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

	struct Access {
		std::lock_guard<std::mutex> guard;
		Value& ref_to_value;

		Access(const Key& key, Bucket& bucket)
			: guard(bucket.mutex)
			, ref_to_value(bucket.map[key]) {
		}
	};

	explicit ConcurrentMap(size_t bucket_count)
		: buckets_(bucket_count) {
	}

	Access operator[](const Key& key) {
		auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
		return { key, bucket };
	}

	std::map<Key, Value> BuildOrdinaryMap() {
		std::map<Key, Value> result;
		for (auto& [mutex, map] : buckets_) {
			std::lock_guard g(mutex);
			result.insert(map.begin(), map.end());
		}
		return result;

		//template <typename Key, typename Value>
		auto ConcurrentMap<Key, Value> Erase(const Key & key) {
			uint64_t tmp_key = static_cast<uint64_t>(key) % buckets_.size();

			std::lock_guard guard(buckets_[tmp_key].mutex);

			return buckets_[tmp_key].map.erase(key);
		}
	}

private:
	std::vector<Bucket> buckets_;
};

//===============================================================//

//====================== ConcurrentSet =========================//

//template <typename Value>
//class ConcurrentSet {
//private:
//	struct Bucket {
//		std::mutex mutex;
//		std::set<Value> set;
//	};
//
//public:
//	static_assert(std::is_integral_v<Value>, "ConcurrentSet supports only integer keys"s);
//
//	struct Access {
//		std::lock_guard<std::mutex> guard;
//		Value& ref_to_value;
//
//		Access(const Value& value, Bucket& bucket)
//			: guard(bucket.mutex)
//			, ref_to_value(bucket.set.insert(value)) {
//		}
//	};
//
//	explicit ConcurrentSet(size_t bucket_count)
//		: buckets_(bucket_count) {
//	}
//
//	/*Access operator[](const Key& key) {
//		auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
//		return { key, bucket };
//	}*/
//
//	std::set<Value> BuildOrdinarySet() {
//		std::set<Value> result;
//		for (auto& [mutex, set] : buckets_) {
//			std::lock_guard g(mutex);
//			result.insert(set.begin(), set.end());
//		}
//		return result;
//	}
//
//private:
//	std::vector<Bucket> buckets_;
//};

//===============================================================//