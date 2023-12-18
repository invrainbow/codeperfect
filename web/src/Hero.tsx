import { Icon } from "@/components/Icon";
import { links } from "@/links";
import { Button } from "./components/Button";
import { Link } from "./components/Link";
import dom from "./components/dom";

export const Hero = () => (
  <dom.div cx="flex flex-col gap-6 md:gap-8 max-w-full leading-relaxed px-4">
    <dom.div cx="md:text-center font-bold text-5xl text-black tracking-tight leading-[1.1] md:leading-none">
      A fast, lightweight Go IDE
    </dom.div>
    <dom.div cx="flex md:text-center flex-col items-stretch self-stretch md:self-auto md:items-center md:flex-row gap-2 md:gap-4 md:justify-center">
      <Button href={links.download}>
        <Icon size={18} icon="Download" />
        Download for Mac
      </Button>
      <Button variant="secondary" href={links.docs}>
        View Docs
        <Icon
          size={18}
          icon="ChevronRight"
          cx="relative group-hover:translate-x-1 transition"
        />
      </Button>
    </dom.div>
    <dom.div cx="md:text-center">
      <dom.span cx="text-slate-500 text-sm font-medium leading-normal md:leading-none inline-block">
        <b>Note:</b> CodePerfect is no longer actively developed, but is now{" "}
        <Link href={links.github}>open source</Link> and free.
      </dom.span>
    </dom.div>
  </dom.div>
);
