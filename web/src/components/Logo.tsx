import { A, Props as AProps } from "./A";
import { Image } from "./Image";
import { Span } from "./Span";

interface Props extends AProps {
  hideText?: boolean;
}

export const Logo = ({ hideText, ...rest }: Props) => (
  <A
    href="/"
    newWindow={false}
    cx="font-bold text-lg text-black no-underline whitespace-nowrap inline-flex flex-shrink-0 items-center"
    {...rest}
  >
    <Image cx="w-auto h-8 inline-block mr-3" src={"/logo.png"} />
    {!hideText && (
      <Span cx="inline-block logo text-lg font-bold">CodePerfect</Span>
    )}
  </A>
);
