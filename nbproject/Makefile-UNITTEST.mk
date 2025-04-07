#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=clang-20
CCC=clang++-20
CXX=clang++-20
FC=gfortran
AS=lld-20

# Macros
CND_PLATFORM=CLang-20-Linux
CND_DLIB_EXT=so
CND_CONF=UNITTEST
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/_cycles.o \
	${OBJECTDIR}/_example.o \
	${OBJECTDIR}/memsafe_test.o


# C Compiler Flags
CFLAGS=

# CC Compiler Flags
CCFLAGS=-std=c++20
CXXFLAGS=-std=c++20

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=-lpthread -lgtest -lgtest_main -lyaml-cpp

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk memsafe_unittest

memsafe_unittest: ${OBJECTFILES}
	${LINK.cc} -o memsafe_unittest ${OBJECTFILES} ${LDLIBSOPTIONS}

${OBJECTDIR}/_cycles.o: _cycles.cpp memsafe_clang.so circleref.memsafe nbproject/Makefile-${CND_CONF}.mk
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.cc) -g -DBUILD_UNITTEST -I. -std=c++20 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe -Xclang -plugin-arg-memsafe -Xclang log -Xclang -plugin-arg-memsafe -Xclang level=warning -Xclang -plugin-arg-memsafe -Xclang circleref-read -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_cycles.o _cycles.cpp

${OBJECTDIR}/_example.o: _example.cpp memsafe_clang.so nbproject/Makefile-${CND_CONF}.mk
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.cc) -g -DBUILD_UNITTEST -I. -std=c++20 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe -Xclang -plugin-arg-memsafe -Xclang log -Xclang -plugin-arg-memsafe -Xclang level=warning -Xclang -plugin-arg-memsafe -Xclang circleref-disable -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_example.o _example.cpp

circleref.memsafe: circleref.memsafe memsafe_clang.so _example.cpp nbproject/Makefile-${CND_CONF}.mk
	@echo "\033[1;46;34m"Building circleref.memsafe"\033[0m"
	clang-20 -std=c++20 -ferror-limit=500 -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe -Xclang -plugin-arg-memsafe -Xclang level=warning -Xclang -plugin-arg-memsafe -Xclang circleref-write -fsyntax-only _example.cpp

memsafe_clang.so: memsafe_clang.cpp memsafe_plugin.h nbproject/Makefile-${CND_CONF}.mk
	@echo "\033[1;46;34m"Building a plugin memsafe_clang.so"\033[0m"
	clang-20 -fPIC -shared -o memsafe_clang.so memsafe_clang.cpp `llvm-config-20 --cppflags --ldflags --system-libs --libs all` -std=c++20  -lyaml-cpp

${OBJECTDIR}/memsafe_test.o: memsafe_test.cpp memsafe_clang.so nbproject/Makefile-${CND_CONF}.mk
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.cc) -g -DBUILD_UNITTEST -I. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/memsafe_test.o memsafe_test.cpp

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}
	${RM} circleref.memsafe
	${RM} memsafe_clang.so

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
