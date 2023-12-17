import { A } from "@/components/A";
import { Icon } from "@/components/Icon";
import { links } from "@/links";
import { IconChevronRight, IconDownload } from "@tabler/icons-react";

export const Hero = () => (
  <div className="flex flex-col gap-6 md:gap-8 max-w-full leading-relaxed px-4">
    <div className="md:text-center font-bold text-5xl text-black tracking-tight leading-[1.1] md:leading-none">
      A fast, lightweight Go IDE
    </div>
    <div className="md:text-center flex flex-col md:flex-row gap-2 md:gap-4 md:justify-center">
      <A
        href={links.download}
        className="btn btn1 justify-center flex md:inline-flex text-center"
      >
        <Icon size={18} className="mr-1" icon={IconDownload} />
        Download for Mac
      </A>
      <A
        href={links.docs}
        className="btn btn2 justify-center flex md:inline-flex text-center"
      >
        View Docs
        <Icon size={18} className="ml-1" icon={IconChevronRight} />
      </A>
    </div>
    <div className="md:text-center">
      <span className="text-slate-500 text-sm font-medium leading-normal md:leading-none inline-block">
        <b>Note:</b> CodePerfect is no longer actively developed, but is now{" "}
        <A href={links.github}>open source</A> and free.
      </span>
    </div>
  </div>
);
