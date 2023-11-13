import { A, Icon } from "@/components";
import { FEATURE_LIST, LINKS } from "@/constants";
import { IconChevronRight, IconDownload } from "@tabler/icons-react";

const BAD_FEATURES = [
  "Electron",
  "JavaScript",
  "language servers",
  "frameworks",
  "pointless abstractions",
];

export default function Home() {
  return (
    <div className="bg-neutral-50 mx-auto w-full">
      <div className="bg-white flex flex-col gap-10 max-w-full leading-relaxed px-8 py-12 md:pt-20 md:pb-24">
        <div>
          <div className="md:text-center font-bold text-5xl text-black tracking-tight leading-[1.1] md:leading-none mb-8">
            A fast, lightweight Go IDE
          </div>
          <div className="text-center leading-relaxed">
            <span className="bg-slate-200 text-slate-600 rounded-full text-sm font-medium py-2 px-4 leading-none">
              <b>Note:</b> CodePerfect is no longer actively developed, but is
              now <A href={LINKS.github}>open source</A> and free.
            </span>
          </div>
        </div>
        <div className="md:text-center flex flex-col md:flex-row gap-4 md:justify-center">
          <A
            href={LINKS.download}
            className="btn btn1 justify-center flex md:inline-flex text-center"
          >
            <Icon size={18} className="mr-1" icon={IconDownload} />
            Download for Mac
          </A>
          <A
            href={LINKS.docs}
            className="btn btn2 justify-center flex md:inline-flex text-center"
          >
            View Docs
            <Icon size={18} className="ml-1" icon={IconChevronRight} />
          </A>
        </div>
      </div>

      <div className="bg-neutral-900 border-gray-100 p-8 py-12 md:pb-24 relative z-10 flex items-center justify-center">
        <div className="max-w-screen-xl flex flex-col lg:flex-row items-center gap-8 md:gap-16">
          <div className="lg:w-1/3 md:mx-0 flex flex-col gap-6 md:gap-8">
            <div className="text-[160%] md:text-[200%] font-extrabold text-black leading-tight">
              {BAD_FEATURES.map((name) => (
                <div key={name} className="text-white md:whitespace-nowrap">
                  No {name}.
                </div>
              ))}
            </div>
            <div className="leading-relaxed text-neutral-400">
              <p>
                CodePerfect is built from scratch in C/C++/OpenGL like a video
                game. It starts instantly, runs at 144 FPS, has near-zero
                latency, and comes with native, full-featured code intelligence
                and integrated debugging with Delve.
              </p>
            </div>
            <div>
              <A className="btn btn3" href={LINKS.docs}>
                See features
              </A>
            </div>
          </div>
          <div className="md:flex-1">
            <A target="_blank" href="/download.png">
              <img
                className="max-w-full shadow-lg rounded-lg overflow-hidden"
                alt="screenshot"
                src="/download.png"
              />
            </A>
          </div>
        </div>
      </div>

      <div className="batteries-included z-10 px-6 md:px-12 py-12 md:py-28">
        <div className="max-w-screen-lg mx-auto flex flex-col md:flex-row gap-8 items-center">
          <div className="md:w-1/3">
            <h1 className="title text-3xl md:text-4xl">
              <div>Batteries included,</div>
              <div>zero configuration.</div>
            </h1>
            <div className="max-w-screen-sm mx-auto mt-4 mb-6 text-lg">
              <p>
                Get the best of both worlds: the speed of Vim, the power of an
                IDE.
              </p>
            </div>
            <A
              href="/features"
              className="btn btn1 btn-lg btn-no-hover justify-center flex md:inline-flex text-center group"
            >
              View Docs
              <Icon
                size={18}
                className="ml-1.5 relative top-[1px] group-hover:translate-x-1 transition"
                icon={IconChevronRight}
              />
            </A>
          </div>

          <div className="hidden md:block flex-1">
            <div className="flex flex-wrap justify-center gap-x-8 gap-y-3 text-[110%] rounded p-6 font-mono">
              {FEATURE_LIST.map((it) => (
                <div
                  key={it.name}
                  className="flex-shrink leading-none text-center text-neutral-400 hover:text-neutral-600 transition-colors"
                >
                  {it.name}
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
