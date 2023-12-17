import { A } from "./A";

interface Props {
  onClick?: () => void;
  hideText?: boolean;
}

export const Logo = ({ onClick, hideText }: Props) => (
  <A
    href="/"
    className="font-bold text-lg text-black no-underline whitespace-nowrap inline-flex flex-shrink-0 items-center"
    onClick={onClick}
  >
    <img
      alt="logo"
      className="w-auto h-8 inline-block mr-3"
      src={"/logo.png"}
    />
    {!hideText && (
      <span className="inline-block logo text-lg font-bold">CodePerfect</span>
    )}
  </A>
);
