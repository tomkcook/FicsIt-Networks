#include "LuaFileSystemAPI.h"

#include "LuaInstance.h"
#include "LuaProcessor.h"

#include "FicsItKernel/FicsItFS/FileSystem.h"

#define LuaFunc(funcName) \
int funcName(lua_State* L) { \
	KernelSystem* kernel = LuaProcessor::luaGetProcessor(L)->getKernel(); \
	FicsItFS::Root* self = kernel->getFileSystem(); \
	if (!self) return luaL_error(L, "component is invalid");

#define LuaFileFuncName(funcName) luaFile ## funcName
#define LuaFileFunc(funcName) \
int LuaFileFuncName(funcName) (lua_State* L) { \
	KernelSystem* kernel = LuaProcessor::luaGetProcessor(L)->getKernel(); \
	LuaFile* self_r = (LuaFile*)luaL_checkudata(L, 1, "File"); \
	if (!self_r) return luaL_error(L, "file is invalid"); \
	LuaFile& self = *self_r; \
	if (self->transfer) { \
		FileSystem::FileMode mode; \
		if (self->transfer->open) { \
			mode = self->transfer->mode; \
			mode = mode & ~FileSystem::FileMode::TRUNC; \
		} else mode = FileSystem::INPUT; \
		self->file = kernel->getFileSystem()->open(kernel->getFileSystem()->unpersistPath(self->path), mode); \
		if (self->transfer->open) { \
			self->file->seek("set", self->transfer->pos); \
			self->transfer = nullptr; \
		} else { \
			self->file->close(); \
		} \
		lua_getfield(L, LUA_REGISTRYINDEX, "FileStreamStorage"); \
        std::set<LuaFile>& streams = *static_cast<std::set<LuaFile>*>(lua_touserdata(L, -1)); \
		streams.insert(self); \
	} \
	FileSystem::SRef<FileSystem::FileStream>& file = self->file; \
	if (!file) return luaL_error(L, "filestream not open");

#define CatchExceptionLua \
	catch (const std::exception& ex) { \
		return luaL_error(L, ex.what()); \
	}

namespace FicsItKernel {
	namespace Lua {
		LuaFunc(makeFileSystem)
			std::string type = luaL_checkstring(L, 1);
			std::string name = luaL_checkstring(L, 2);
			FileSystem::SRef<FileSystem::Device> device;
			if (type == "tmpfs") {
				device = new FileSystem::MemDevice();
			} else return luaL_argerror(L, 1, "No valid FileSystem Type");
			if (!device.isValid()) luaL_argerror(L, 1, "Invalid Device");
			FileSystem::SRef<FicsItFS::DevDevice> dev = self->getDevDevice();
			try {
				lua_pushboolean(L, dev.isValid() ? dev->addDevice(device, name) : false);
			} CatchExceptionLua
			return 1;
		}

		LuaFunc(removeFileSystem)
			std::string name = luaL_checkstring(L, 1);
			FileSystem::SRef<FicsItFS::DevDevice> dev = self->getDevDevice();
			if (dev.isValid()) {
				try {
					auto devices = dev->getDevices();
					auto device = devices.find(name);
					if (device != devices.end() && dynamic_cast<FileSystem::MemDevice*>(device->second.get())) {
						lua_pushboolean(L, dev->removeDevice(device->second));
						return 1;
					}
				} CatchExceptionLua
			}
			lua_pushboolean(L, false);
			return 1;
		}

		LuaFunc(initFileSystem)
			std::string path = luaL_checkstring(L, 1);
			lua_pushboolean(L, kernel->initFileSystem(path));
			return 1;
		}

		LuaFunc(open)
			std::string mode = "r";
			if (lua_isstring(L, 2)) mode = lua_tostring(L, 2);
			FileSystem::FileMode m;
			if (mode == "r") m = FileSystem::INPUT;
			else if (mode == "w") m = FileSystem::OUTPUT | FileSystem::TRUNC;
			else if (mode == "a") m = FileSystem::OUTPUT | FileSystem::APPEND;
			else if (mode == "+r") m = FileSystem::INPUT | FileSystem::OUTPUT;
			else if (mode == "+w") m = FileSystem::INPUT | FileSystem::OUTPUT | FileSystem::TRUNC;
			else if (mode == "+a") m = FileSystem::INPUT | FileSystem::OUTPUT | FileSystem::APPEND;
			else luaL_argerror(L, 2, "is not valid file mode");
			try {
				std::string path = luaL_checkstring(L, 1);
				luaFile(L, self->open(path, m), self->persistPath(path));
			} CatchExceptionLua
			return 1;
		}

		LuaFunc(createDir)
			auto path = luaL_checkstring(L, 1);
			auto all = lua_toboolean(L, 2);
			try {
				lua_pushboolean(L, self->createDir(path, all) == 0);
			} CatchExceptionLua
			return 1;
		}

		LuaFunc(remove)
			auto path = luaL_checkstring(L, 1);
			bool all = lua_toboolean(L, 2);
			try {
				lua_pushboolean(L, self->remove(path, all));
			} CatchExceptionLua
			return 1;
		}

		LuaFunc(move)
			auto from = luaL_checkstring(L, 1);
			auto to = luaL_checkstring(L, 2);
			try {
				lua_pushboolean(L, self->move(from, to) == 0);
			} CatchExceptionLua
			return 1;
		}

		LuaFunc(rename)
			auto from = luaL_checkstring(L, 1);
			auto to = luaL_checkstring(L, 2);
			try {
				lua_pushboolean(L, self->rename(from, to));
			} CatchExceptionLua
			return 1;
		}

		LuaFunc(exists)
			auto path = luaL_checkstring(L, 1);
			try {
				lua_pushboolean(L, self->get(path).isValid());
			} CatchExceptionLua
			return 1;
		}

		LuaFunc(childs)
			auto path = luaL_checkstring(L, 1);
			std::unordered_set<FileSystem::NodeName> childs;
			try {
				childs = self->childs(path);
			} CatchExceptionLua
			lua_newtable(L);
			int i = 1;
			for (auto& child : childs) {
				lua_pushstring(L, child.c_str());
				lua_seti(L, -2, i++);
			}
			return 1;
		}

		LuaFunc(isFile)
			auto path = luaL_checkstring(L, 1);
			try {
				lua_pushboolean(L, !!dynamic_cast<FileSystem::File*>(self->get(path).get()));
			} CatchExceptionLua
			return 1;
		}

		LuaFunc(isDir)
			auto path = luaL_checkstring(L, 1);
			try {
				lua_pushboolean(L, !!dynamic_cast<FileSystem::Directory*>(self->get(path).get()));
			} CatchExceptionLua
			return 1;
		}

		LuaFunc(mount)
			auto devPath = luaL_checkstring(L, 1);
			auto mountPath = luaL_checkstring(L, 2);
			try {
				lua_pushboolean(L, FileSystem::DeviceNode::mount(*self, devPath, mountPath));
			} CatchExceptionLua
			return 1;
		}

		LuaFunc(unmount)
			auto mountPath = luaL_checkstring(L, 1);
			try {
				lua_pushboolean(L, self->unmount(mountPath));
			} CatchExceptionLua
			return 1;
		}

		LuaFunc(doFile)
			FileSystem::SRef<FileSystem::File> p;
			try {
				p = self->get(luaL_checkstring(L, 1));
			} CatchExceptionLua
			if (!p.isValid()) return luaL_argerror(L, 1, "path is no valid file");
			FileSystem::SRef<FileSystem::FileStream> s;
			try {
				s = p->open(FileSystem::INPUT);
			} CatchExceptionLua
			if (!s.isValid()) return luaL_error(L, "not able to create filestream");
			int n = lua_gettop(L);
			std::string code;
			try {
				code = s->readAll();
			} CatchExceptionLua
			luaL_dofile(L, code.c_str());
			try {
				s->close();
			} CatchExceptionLua
			return lua_gettop(L) - n;
		}

		LuaFunc(loadFile)
			FileSystem::SRef<FileSystem::File> p;
			try {
				p = self->get(luaL_checkstring(L, 1));
			} CatchExceptionLua
			if (!p.isValid()) return luaL_argerror(L, 1, "path is no valid file");
			FileSystem::SRef<FileSystem::FileStream> s;
			try {
				s = p->open(FileSystem::INPUT);
			} CatchExceptionLua
			if (!s.isValid()) return luaL_error(L, "not able to create filestream");
			std::string code;
			try {
				code = s->readAll();
			} CatchExceptionLua
			luaL_loadfile(L, code.c_str());
			try {
				s->close();
			} CatchExceptionLua
			return 1;
		}

		static const luaL_Reg luaFileSystemLib[] = {
			{"makeFileSystem", makeFileSystem},
			{"removeFileSystem", removeFileSystem},
			{"initFileSystem", initFileSystem},
			{"open", open},
			{"createDir", createDir},
			{"remove", remove},
			{"move", move},
			{"rename", rename},
			{"exists", exists},
			{"childs", childs},
			{"isFile", isFile},
			{"isDir", isDir},
			{"mount", mount},
			{"unmount", unmount},
			{"doFile", doFile},
			{"loadFile", loadFile},
			{NULL,NULL}
		};

		LuaFileFunc(Close)
			try {
				file->close();
			} CatchExceptionLua
			return 0;
		}

		LuaFileFunc(Write)
			auto s = lua_gettop(L);
			for (int i = 2; i <= s; ++i) {
				size_t str_len = 0;
				const char* str = luaL_checklstring(L, i, &str_len);
				try {
					file->write(std::string(str, str_len));
				} CatchExceptionLua
			}
			return 0;
		}

		LuaFileFunc(Flush)
			try {
				file->flush();
			} CatchExceptionLua
			return 0;
		}

		LuaFileFunc(Read)
			auto args = lua_gettop(L);
			for (int i = 2; i <= args; ++i) {
				bool invalidFormat = false;
				try {
					if (lua_isnumber(L, i)) {
						if (file->isEOF()) lua_pushnil(L);
						auto n = lua_tointeger(L, i);
						std::string s = file->readChars(n);
						lua_pushlstring(L, s.c_str(), s.size());
					} else {
						char fo = 'l';
						if (lua_isstring(L, i)) {
							std::string s = lua_tostring(L, i);
							if (s.size() != 2 || s[0] != '*') fo = '\0';
							fo = s[1];
						}
						switch (fo) {
						case 'n':
						{
							lua_pushnumber(L, file->readNumber());
							break;
						} case 'a':
						{
							std::string s = file->readAll();
							lua_pushlstring(L, s.c_str(), s.size());
							break;
						} case 'l':
						{
							if (!file->isEOF()) {
								std::string s = file->readLine();
								lua_pushlstring(L, s.c_str(), s.size());
							} else lua_pushnil(L);
							break;
						}
						default:
							invalidFormat = true;
						}
					}
				} catch (const std::exception& ex) {
					luaL_error(L, ex.what());
				}
				if (invalidFormat) return luaL_argerror(L, i, "no valid format");
			}
			return args - 1;
		}

		LuaFileFunc(ReadLine)
			try {
				if (file->isEOF()) lua_pushnil(L);
				else lua_pushstring(L, file->readLine().c_str());
			} CatchExceptionLua
			return 1;
		}

		LuaFileFunc(Lines)
			auto f = (LuaFile*)luaL_checkudata(L, 1, "File");
			lua_pushcclosure(L, luaFileReadLine, 1);
			return 1;
		}

		LuaFileFunc(Seek)
			LuaFile& f = *(LuaFile*)luaL_checkudata(L, 1, "File");
			std::string w = "cur";
			std::int64_t off = 0;
			if (lua_isstring(L, 2)) w = lua_tostring(L, 2);
			if (lua_isinteger(L, 3)) off = lua_tointeger(L, 3);
			int64_t seek;
			try {
				seek = f->file->seek(w, off);
			} CatchExceptionLua
			lua_pushinteger(L, seek);
			return 1;
		}

		LuaFileFunc(String)
			std::string text;
			try {
				text = file->readAll();
			} CatchExceptionLua
			lua_pushstring(L, text.c_str());
			return 1;
		}

		int luaFileUnpersist(lua_State* L) {
			KernelSystem* kernel = LuaProcessor::luaGetProcessor(L)->getKernel();
			const bool valid = lua_toboolean(L, lua_upvalueindex(1));
			const std::string path = kernel->getFileSystem()->unpersistPath(lua_tostring(L, lua_upvalueindex(2)));
			if (valid) {
				const bool open = lua_toboolean(L, lua_upvalueindex(3));
				FileSystem::SRef<FileSystem::FileStream> stream;
				if (open) {
					FileSystem::FileMode mode = static_cast<FileSystem::FileMode>((int)lua_tonumber(L, lua_upvalueindex(4)));
					mode = mode & ~FileSystem::FileMode::TRUNC;
					stream = kernel->getFileSystem()->open(path, mode);
					stream->seek("set", static_cast<int>(lua_tonumber(L, lua_upvalueindex(5))));
				} else {
					stream = nullptr;
				}
				luaFile(L, stream, path);
			} else luaFile(L, nullptr, path);
			return 1;
		}

		int luaFilePersist(lua_State* L) {
			LuaFile& f = *static_cast<LuaFile*>(luaL_checkudata(L, -1, "File"));
			if (f->transfer) {
				lua_pushboolean(L, true);
				lua_pushstring(L, f->path.c_str());
				lua_pushboolean(L, f->transfer->open);
				lua_pushinteger(L, f->transfer->mode);
				lua_pushinteger(L, f->transfer->pos);
				lua_pushcclosure(L, luaFileUnpersist, 5);
			} else {
				lua_pushboolean(L, false);
				lua_pushstring(L, f->path.c_str());
				lua_pushcclosure(L, luaFileUnpersist, 2);
			}
			return 1;
		}
		
		int luaFileGC(lua_State * L) {
			LuaFile& f = *static_cast<LuaFile*>(luaL_checkudata(L, 1, "File"));
			try {
				if (f->file) f->file->close();
			} CatchExceptionLua
			f.~LuaFile();
			return 0;
		}

		static const luaL_Reg luaFileLib[] = {
			{"close", luaFileClose},
			{"write", luaFileWrite},
			{"flush", luaFileFlush},
			{"read", luaFileRead},
			{"lines", luaFileLines},
			{"seek", luaFileSeek},
			{"__tostring", luaFileString},
			{"__persist", luaFilePersist},
			{"__gc", luaFileGC},
			{NULL, NULL}
		};

		void luaFile(lua_State* L, FileSystem::SRef<FileSystem::FileStream> file, const std::string& path) {
			auto f = static_cast<LuaFile*>(lua_newuserdata(L, sizeof(LuaFile)));
			luaL_setmetatable(L, "File");
			new (f) LuaFile(new LuaFileContainer());
			f->get()->file = file;
			f->get()->path = path;
			lua_getfield(L, LUA_REGISTRYINDEX, "FileStreamStorage");
			if (file.isValid()) {
				std::set<LuaFile>& streams = *static_cast<std::set<LuaFile>*>(lua_touserdata(L, -1));
				streams.insert(*f);
			}
			lua_pop(L, 1);
		}

		void setupFileSystemAPI(lua_State * L) {
			PersistSetup("FileSystem", -2);
			
			luaL_newlibtable(L, luaFileSystemLib);
			luaL_setfuncs(L, luaFileSystemLib, 0);
			PersistTable("Lib", -1);
			lua_setglobal(L, "filesystem");

			luaL_newmetatable(L, "File");
			lua_pushvalue(L, -1);
			lua_setfield(L, -2, "__index");
			luaL_setfuncs(L, luaFileLib, 0);
			PersistTable("File", -1);
			lua_pop(L, 1);
			lua_pushcfunction(L, luaFileUnpersist);
			PersistValue("FileUnpersist");
		}
	}
}
