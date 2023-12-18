import { Link, Props as LinkProps } from "./Link";
import dom from "./dom";

interface Props extends LinkProps {
  hideText?: boolean;
}

export const Logo = ({ hideText, ...rest }: Props) => (
  <Link
    href="/"
    newWindow={false}
    cx="font-bold text-lg text-black no-underline whitespace-nowrap inline-flex flex-shrink-0 items-center"
    {...rest}
  >
    <dom.img cx="w-auto h-8 inline-block mr-3" src={"/logo.png"} />
    {!hideText && (
      <dom.span cx="inline-block logo text-lg font-bold">CodePerfect</dom.span>
    )}
  </Link>
);
