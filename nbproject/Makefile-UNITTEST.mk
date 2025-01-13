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
CC=clang-19
CCC=clang++-19
CXX=clang++-19
FC=gfortran
AS=lld-19

# Macros
CND_PLATFORM=CLang-19-Linux
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
	${OBJECTDIR}/test/_example.o \
	${OBJECTDIR}/test/memsafe_test.o \
	${OBJECTDIR}/test/unittest.o


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
LDLIBSOPTIONS=-lpthread -lgtest

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/memsafe

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/memsafe: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.cc} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/memsafe ${OBJECTFILES} ${LDLIBSOPTIONS}

${OBJECTDIR}/test/_example.o: test/_example.cpp memsafe_clang.so nbproject/Makefile-${CND_CONF}.mk
	${MKDIR} -p ${OBJECTDIR}/test
	${RM} "$@.d"
	$(COMPILE.cc) -g -DBUILD_UNITTEST -I. -Xclang -load -Xclang ./memsafe_clang.so -Xclang -add-plugin -Xclang memsafe -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/test/_example.o test/_example.cpp

${OBJECTDIR}/test/memsafe_test.o: test/memsafe_test.cpp memsafe_clang.so nbproject/Makefile-${CND_CONF}.mk
	${MKDIR} -p ${OBJECTDIR}/test
	${RM} "$@.d"
	$(COMPILE.cc) -g -DBUILD_UNITTEST -I. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/test/memsafe_test.o test/memsafe_test.cpp

${OBJECTDIR}/test/unittest.o: test/unittest.cpp nbproject/Makefile-${CND_CONF}.mk
	${MKDIR} -p ${OBJECTDIR}/test
	${RM} "$@.d"
	$(COMPILE.cc) -g -DBUILD_UNITTEST -I. -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/test/unittest.o test/unittest.cpp

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
