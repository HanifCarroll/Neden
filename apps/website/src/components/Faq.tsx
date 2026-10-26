import {
  Accordion,
  AccordionContent,
  AccordionItem,
  AccordionTrigger,
} from "@/components/ui/accordion"
import { questions } from "@/data/faq"
export default function Faq() {
  return (
    <Accordion type="single" collapsible defaultValue="question-0">
      {questions.map(({ question, answer }, index) => (
        <AccordionItem value={`question-${index}`} key={question}>
          <AccordionTrigger className="py-6 text-base font-medium">
            {question}
          </AccordionTrigger>
          <AccordionContent className="pb-6 text-base leading-relaxed text-muted-foreground">
            {answer}
          </AccordionContent>
        </AccordionItem>
      ))}
    </Accordion>
  )
}
