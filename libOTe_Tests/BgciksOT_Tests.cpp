#include "BgciksOT_Tests.h"

#include "libOTe/DPF/BgicksPprf.h"
#include "libOTe/TwoChooseOne/BgciksOtExtSender.h"
#include "libOTe/TwoChooseOne/BgciksOtExtReceiver.h"
#include <cryptoTools/Common/Log.h>
#include <cryptoTools/Network/IOService.h>
using namespace oc;


void bitShift_test(const CLP& cmd)
{

	//u64 nBits = ;
	u64 n = cmd.getOr("n", 10);// (nBits + 127) / 128;
	u64 t = cmd.getOr("t", 10);

	PRNG prng(toBlock(cmd.getOr("seed", 0)));

	std::vector<block> dest(n), in;
	prng.get(dest.data(), dest.size());



	//std::cout << "a " << (_mm_slli_epi64(AllOneBlock, 20)) << std::endl;
	//std::cout << "b " << (_mm_srli_epi64(AllOneBlock, 20)) << std::endl;

	for (u64 i = 0; i < t; ++i)
	{
		u8 bitShift = prng.get<u8>() % 128;

		u64 inSize = std::max<u64>(1, n + (i & 1 ? 1 : -1));
		u64 inBits = std::min<u64>(n * 128, inSize * 128 - bitShift);

		in.resize(inSize);
		prng.get(in.data(), in.size());


		memset(dest.data(), 0, dest.size() * 16);

		BitVector dv((u8*)dest.data(), n * 128);
		BitVector iv, ivt((u8*)in.data(), in.size() * 128);
		iv.append((u8*)in.data(), inBits, bitShift);
		iv.resize(n * 128, 0);
		auto dv1 = dv;

		dv ^= iv;

		bitShiftXor(dest, in, bitShift);


		BitVector dv2((u8*)dest.data(), n * 128);

		if (dv != dv2)
		{
			auto b = (bitShift > 64) ? 128 - bitShift : 64 - bitShift;

			std::cout << "\n" << int(bitShift) << "\n";
			std::cout << " i* " << ivt << std::endl;
			std::cout << " i  " << iv << std::endl;
			std::cout << " d  " << dv1 << std::endl;

			std::cout << "   " << std::string(b, ' ') << std::string(bitShift, 'x') << '\n';
			std::cout << "act " << dv2 << std::endl;
			std::cout << "exp " << dv << std::endl;
			std::cout << "    " << (dv2 ^ dv) << std::endl;
			throw RTE_LOC;
		}

	}


}

void clearBits(span<block> in, u64 idx)
{
	auto p = (u8*)in.data() + idx / 8;
	auto e = (u8*)in.data() + in.size() * 16;

	if (idx & 7)
	{
		*p++ &= (1 << (idx & 7)) - 1;
	}

	while (p != e)
		* p++ = 0;


	BitVector test((u8*)in.data(), in.size() * 128);
	for (u64 i = idx; i < test.size(); ++i)
		if (test[i])
			throw RTE_LOC;
}


void modp_test(const CLP & cmd)
{
	//u64 n = cmd.getOr("n", 100);
	//u64 c = cmd.getOr("c", 2);
	u64 t = cmd.getOr("t", 10);


	PRNG prng(toBlock(cmd.getOr("seed", 0)));

	auto iBits = cmd.getOr("c", 1026);
	auto nBits = cmd.getOr("n", 223);

	auto n = (nBits + 127) / 128;
	auto c = (iBits + nBits - 1) / nBits;

	std::vector<block> dest(n), in((iBits + 127) / 128);

	for (u64 i = 0; i < dest.size(); ++i)
	{
		u64 p = nBits;// -(prng.get<u64>() % 128);

		prng.get(in.data(), in.size());
		memset(in.data(), -1, in.size() * 16);
		clearBits(in, iBits);

		memset(dest.data(), 0, dest.size() * 16);

		BitVector dv((u8*)in.data(), p);
		BitVector iv;

		//std::cout << "\nin[0] = " << dv << std::endl;

		for (u64 j = 1; j < c; ++j)
		{
			auto rem = std::min<u64>(p, iBits - j * p);
			iv.resize(0);
			iv.append((u8*)in.data(), rem, j * p);

			//std::cout << "in["<<j<<"] = " << iv << std::endl;

			iv.resize(p, 0);
			dv ^= iv;
		}
		//std::cout << "out   = " << dv << std::endl;



		modp(dest, in, p);


		BitVector dv2((u8*)dest.data(), p);

		if (dv != dv2)
		{
			//auto b = (bitShift > 64) ? 128 - bitShift : 64 - bitShift;
			auto diff = (dv2 ^ dv);
			std::cout << "\n" << p << "\n";
			//std::cout << "   "
			//    << std::string(b, ' ')
			//    << std::string(bitShift, 'x') << '\n';
			std::cout << "act     " << dv2 << std::endl;
			std::cout << "exp     " << dv << std::endl;
			std::cout << "f       " << diff << std::endl;

			for (u64 i = 0; i < diff.size(); ++i)
				if (diff[i])
					std::cout << " " << i;
			std::cout << std::endl;

			throw RTE_LOC;
		}

		//std::cout << dv2 << std::endl;

	}

}

void BgciksOT_Test(const CLP & cmd)
{

	IOService ios;
	Session s0(ios, "localhost:1212", SessionMode::Server);
	Session s1(ios, "localhost:1212", SessionMode::Client);

	BgciksOtExtSender sender;
	BgciksOtExtReceiver recver;
	u64 n = cmd.getOr("n", 100);
	bool verbose = cmd.getOr("v", 0) > 1;
	u64 threads = cmd.getOr("t", 4);
	u64 s = cmd.getOr("s", 4);
	u64 sec = cmd.getOr("sec", 80);
	bool mal = cmd.isSet("mal");

	std::vector<Channel> chls0(threads), chls1(threads);

	for (u64 i = 0; i < threads; ++i)
	{
		chls0[i] = s0.addChannel();
		chls1[i] = s1.addChannel();
	}


	gUseBgicksPprf = cmd.getOr("fakesetup", false) == false;
	PRNG prng(toBlock(cmd.getOr("seed", 0)));
	PRNG prng1(toBlock(cmd.getOr("seed1", 1)));
	Timer timer;
	sender.setTimer(timer);
	recver.setTimer(timer);

	auto t = std::thread([&]() {recver.genBase(n, chls1[0], prng, s, sec, mal, BgciksBaseType::None, threads); });
	sender.genBase(n, chls0[0], prng1, s, sec, mal, BgciksBaseType::None, threads);
	t.join();

	std::vector<block> messages2(n);
	BitVector choice;
	std::vector<std::array<block, 2>> messages(n);

	sender.send(messages, prng, chls0);
	recver.receive(messages2, choice, prng, chls1);
	bool passed = true;
	BitVector act(n);

	choice.resize(n);
	auto actHam = 0;
	auto retHam = 0;
	for (u64 i = 0; i < n; ++i)
	{
		std::array<bool, 2> eqq{ eq(messages2[i], messages[i][0]),eq(messages2[i], messages[i][1]) };
		if (eqq[choice[i]] == false || eqq[choice[i] ^ 1] == true)
		{
			passed = false;
			if (verbose)
				std::cout << Color::Pink;
		}
		if (eqq[0] == false && eqq[1] == false)
		{
			passed = false;
			if (verbose)
				std::cout << Color::Red;
		}

		if (verbose)
			std::cout << i << " " << messages2[i] << " " << messages[i][0] << " " << messages[i][1] << " " << int(choice[i]) << std::endl << Color::Default;

		if (eq(messages2[i], messages[i][1]))
			act[i] = 1;
	}

	if (verbose)
	{
		std::cout << "act ham " << act.hammingWeight() << " " << act.size() << std::endl;
		std::cout << "ret ham " << choice.hammingWeight() << " " << choice.size() << std::endl;
	}

	if (cmd.isSet("v"))
		std::cout << timer << std::endl;

	if (passed == false)
		throw RTE_LOC;
}



void BgciksPprf_Test(const CLP & cmd)
{
	u64 depth = cmd.getOr("d", 3);;
	u64 domain = 1ull << depth;
	auto threads = cmd.getOr("t", 3);
	u64 numPoints = cmd.getOr("s",8);

	PRNG prng(ZeroBlock);

	IOService ios;
	Session s0(ios, "localhost:1212", SessionMode::Server);
	Session s1(ios, "localhost:1212", SessionMode::Client);


	std::vector<Channel> chls0(threads), chls1(threads);

	for (u64 i = 0; i < threads; ++i)
	{
		chls0[i] = s0.addChannel();
		chls1[i] = s1.addChannel();
	}


	BgicksMultiPprfSender sender;
	BgicksMultiPprfReceiver recver;

	sender.configure(domain, numPoints);
	recver.configure(domain, numPoints);

	auto numOTs = sender.baseOtCount();
	std::vector<std::array<block, 2>> sendOTs(numOTs);
	std::vector<block> recvOTs(numOTs);
	BitVector recvBits(numOTs);
	recvBits.randomize(prng);

	//prng.get(sendOTs.data(), sendOTs.size());
	//sendOTs[cmd.getOr("i",0)] = prng.get();

	//recvBits[16] = 1;
	for (u64 i = 0; i < numOTs; ++i)
	{
		//recvBits[i] = 0;
		recvOTs[i] = sendOTs[i][recvBits[i]];
	}
	sender.setBase(sendOTs);
	recver.setBase(recvOTs, recvBits);

	Matrix<block> sOut(domain, numPoints);
	Matrix<block> rOut(domain, numPoints);
	std::vector<u64> points(numPoints);
	recver.getPoints(points);

	sender.expand(chls0, CCBlock, prng, sOut, false, false);
	recver.expand(chls1, prng, rOut, false, false);
	bool failed = false;


	for (u64 j = 0; j < numPoints; ++j)
	{

		for (u64 i = 0; i < domain; ++i)
		{

			auto exp = sOut(i, j);
			if (points[j] == i)
				exp = exp ^ CCBlock;

			if (neq(exp, rOut(i, j)))
			{
				failed = true;

				if (cmd.isSet("v"))
					std::cout << Color::Red;
			}
			if (cmd.isSet("v"))
				std::cout << "r[" << j << "][" << i << "] " << exp << " " << rOut(i, j) << std::endl << Color::Default;
		}
	}

	if (failed)
		throw RTE_LOC;
}

void BgciksPprf_trans_Test(const CLP & cmd)
{
	//u64 depth = 6;
	//u64 domain = 13;// (1ull << depth) - 7;
	//u64 numPoints = 40;

	u64 domain = cmd.getOr("d", 334);
	auto threads = cmd.getOr("t", 3);
	u64 numPoints = cmd.getOr("s", 5) *8;

	PRNG prng(ZeroBlock);

	IOService ios;
	Session s0(ios, "localhost:1212", SessionMode::Server);
	Session s1(ios, "localhost:1212", SessionMode::Client);

	std::vector<Channel> chls0(threads), chls1(threads);
	for (u64 i = 0; i < threads; ++i)
	{
		chls0[i] = s0.addChannel();
		chls1[i] = s1.addChannel();
	}




	BgicksMultiPprfSender sender;
	BgicksMultiPprfReceiver recver;

	sender.configure(domain, numPoints);
	recver.configure(domain, numPoints);

	auto numOTs = sender.baseOtCount();
	std::vector<std::array<block, 2>> sendOTs(numOTs);
	std::vector<block> recvOTs(numOTs);
	BitVector recvBits(numOTs);
	recvBits.randomize(prng);

	//recvBits[16] = 1;
	for (u64 i = 0; i < numOTs; ++i)
	{
		//recvBits[i] = 0;
		recvOTs[i] = sendOTs[i][recvBits[i]];
	}
	sender.setBase(sendOTs);
	recver.setBase(recvOTs, recvBits);

	auto cols = (numPoints * domain + 127) / 128;
	Matrix<block> sOut(128, cols);
	Matrix<block> rOut(128, cols);
	std::vector<u64> points(numPoints);
	recver.getTransposedPoints(points);

	sender.expand(chls0, AllOneBlock, prng, sOut, true, true);
	recver.expand(chls1, prng, rOut, true, true);
	bool failed = false;

	Matrix<block> out(128, cols);
	Matrix<block> outT(numPoints * domain, 1);

	if (cmd.getOr("v", 0) > 1)
		std::cout << sender.mDomain << " " << sender.mPntCount <<
		" " << sOut.rows() << " " << sOut.cols() << std::endl;
	for (u64 i = 0; i < cols; ++i)
	{
		for (u64 j = 0; j < 128; ++j)
		{
			out(j, i) = (sOut(j, i) ^ rOut(j, i));
			//if (cmd.isSet("v"))
			//	std::cout << "r[" << i << "][" << j << "] " << out(j,i)  << " ~ " << rOut(j, i) << std::endl << Color::Default;
		}
	}
	sse_transpose(MatrixView<block>(out), MatrixView<block>(outT));

	for (u64 i = 0; i < outT.rows(); ++i)
	{

		auto f = std::find(points.begin(), points.end(), i) != points.end();

		auto exp = f ? AllOneBlock : ZeroBlock;

		if (neq(outT(i), exp))
		{
			failed = true;

			if (cmd.getOr("v", 0) > 1)
				std::cout << Color::Red;
		}
		if (cmd.getOr("v", 0) > 1)
			std::cout << i << " " << outT(i) << " " << exp << std::endl << Color::Default;
	}

	if (failed)
		throw RTE_LOC;
}


//void BgciksOT_mul_Test(const CLP& cmd)
//{
//    gf2x_mod_mul()
//}